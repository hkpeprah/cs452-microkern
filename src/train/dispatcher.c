#include <dispatcher.h>
#include <train_task.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <stdlib.h>
#include <clock.h>
#include <term.h>
#include <train.h>
#include <sensor_server.h>
#include <conductor.h>
#include <track_reservation.h>

static uint32_t train_dispatcher_tid = -1;
static uint32_t num_of_trains = 0;

typedef enum {
    TRM_ADD = 77,
    TRM_ADD_AT,
    TRM_GOTO_STOP,
    TRM_GOTO,
    TRM_GOTO_AFTER,
    TRM_AUX,
    TRM_RV,
    TRM_GET_SPEED,
    TRM_SPEED,
    TRM_GET_TRACK_NODE,
    TRM_RESERVE_TRACK,
    TRM_RESERVE_TRACK_DIST,
    TRM_RELEASE_TRACK,
    TRM_CREATE_TRAIN,
    TRM_MOVE
} DispatcherMessageType;

typedef struct DispatcherMessage {
    DispatcherMessageType type;
    uint32_t tr;
    int arg0;
    int arg1;
    int arg2;
} DispatcherMessage_t;

typedef struct {
    uint32_t train;
    uint32_t tr_number : 8;
    int conductor;
} DispatcherNode_t;


static int SendDispatcherMessage(DispatcherMessageType type, uint32_t tr, int arg0, int arg1, int arg2) {
    DispatcherMessage_t msg;
    int status, bytes;
    uint32_t dispatcher;

    status = 0;
    dispatcher = WhoIs(TRAIN_DISPATCHER);
    if (type == TRM_SPEED && arg0 > TRAIN_MAX_SPEED) {
        status = INVALID_SPEED;
    }

    msg.type = type;
    msg.tr = tr;
    msg.arg0 = arg0;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    if (status != 0) {
        return status;
    } else if ((bytes = Send(dispatcher, &msg, sizeof(msg), &status, sizeof(status))) < 0) {
        error("Dispatcher: Error: Error in send to %u returned %d", dispatcher, bytes);
        return -1;
    }
    return status;
}


int DispatchTrainAuxiliary(uint32_t tr, uint32_t aux) {
    return SendDispatcherMessage(TRM_AUX, tr, aux, 0, 0);
}


int DispatchTrainSpeed(uint32_t tr, uint32_t speed) {
    return SendDispatcherMessage(TRM_SPEED, tr, speed, 0, 0);
}


int DispatchRoute(uint32_t tr, uint32_t sensor, uint32_t dist) {
    return SendDispatcherMessage(TRM_GOTO, tr, sensor, dist, 0);
}


int DispatchAddTrain(uint32_t tr) {
    return SendDispatcherMessage(TRM_ADD, tr, 0, 0, 0);
}


int DispatchAddTrainAt(uint32_t tr, char module, uint32_t id) {
    return SendDispatcherMessage(TRM_ADD_AT, tr, module, id, 0);
}


int DispatchTrainReverse(uint32_t tr) {
    return SendDispatcherMessage(TRM_RV, tr, 0, 0, 0);
}


int DispatchStopRoute(uint32_t tr) {
    return SendDispatcherMessage(TRM_GOTO_STOP, tr, 0, 0, 0);
}


track_node *DispatchGetTrackNode(uint32_t id) {
    return (track_node*)SendDispatcherMessage(TRM_GET_TRACK_NODE, 0, id, 0, 0);
}


int DispatchTrainMove(uint32_t id, uint32_t dist) {
    return SendDispatcherMessage(TRM_MOVE, id, dist, 0, 0);
}


track_node *DispatchReserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist) {
    int result = SendDispatcherMessage(TRM_RESERVE_TRACK_DIST, tr, (int) track, n, (int)dist);
    if (result < 0) {
        error("DispatchReserveTrackDist: error: %d", result);
        return NULL;
    }

    return (track_node*) result;
}


track_node *DispatchReserveTrack(uint32_t tr, track_node **track, uint32_t n) {
    int result = SendDispatcherMessage(TRM_RESERVE_TRACK, tr, (int) track, n, 0);
    if (result < 0) {
        error("DispatchReserveTrack: error: %d", result);
        return NULL;
    }

    return (track_node*) result;
}


track_node *DispatchReleaseTrack(uint32_t tr, track_node **track, uint32_t n) {
    int result = SendDispatcherMessage(TRM_RELEASE_TRACK, tr, (int) track, n, 0);
    if (result < 0) {
        error("DispatchReleaseTrack: error: %d", result);
        return NULL;
    }

    return (track_node*) result;
}


static DispatcherNode_t *addDispatcherNode(DispatcherNode_t *nodes, unsigned int tid, unsigned int tr) {
    unsigned int i;
    i = num_of_trains;
    nodes[i].train = tid;
    nodes[i].tr_number = tr;
    nodes[i].conductor = -1;
    notice("Dispatcher: Created Train %u with tid %u", tr, nodes[i].train);
    num_of_trains++;
    return &nodes[i];
}


static int findSensorForTrain(unsigned int tr) {
    int sensorNum;
    trainSpeed(tr, 3);
    sensorNum = WaitAnySensor();
    trainSpeed(tr, 0);
    return sensorNum;
}


static void TrainCreateCourier() {
    DispatcherMessage_t req;
    int callee, tid, parent;
    int sensor;

    parent = MyParentTid();
    Receive(&callee, &req, sizeof(req));
    if (callee != parent) {
        error("TrainCreateCourier: Error: Received request from %u, expected %u", callee, parent);
        Exit();
    }
    Reply(callee, NULL, 0);
    if ((sensor = req.arg0) == -1) {
        sensor = findSensorForTrain(req.tr);
    }

    tid = TrCreate(6, req.tr, DispatchGetTrackNode(sensor));
    if (tid >= 0) {
        debug("TrainCreateCourier: Creating Train %u with Tid %d", req.tr, tid);
        req.type = TRM_CREATE_TRAIN;
        req.tr = req.tr;
        req.arg0 = tid;
        Send(parent, &req, sizeof(req), NULL, 0);
    } else {
        error("TrainCreateCourier: Error: Failed to create new Train task");
    }
    Exit();
}


static int addDispatcherTrain(unsigned int tr, int sensor) {
    DispatcherMessage_t req;
    unsigned int tid;

    if (num_of_trains < NUM_OF_TRAINS) {
        tid = Create(9, TrainCreateCourier);
        req.type = TRM_ADD;
        req.tr = tr;
        req.arg0 = sensor;
        Send(tid, &req, sizeof(req), NULL, 0);
        return tid;
    }
    return -1;
}


static DispatcherNode_t *getDispatcherNode(DispatcherNode_t *nodes, uint32_t tr) {
    uint32_t i;
    for (i = 0; i < num_of_trains; ++i) {
        if (nodes[i].tr_number == tr) {
            return &nodes[i];
        }
    }
    return NULL;
}


void Dispatcher() {
    int callee, status;
    unsigned int CreateCourier;
    DispatcherMessage_t request;
    track_node track[TRACK_MAX];
    DispatcherNode_t trains[NUM_OF_TRAINS], *node;

    init_track(track);
    RegisterAs(TRAIN_DISPATCHER);
    train_dispatcher_tid = MyTid();
    notice("Dispatcher: Tid %u", train_dispatcher_tid);
    num_of_trains = 0;
    CreateCourier = -1;
    setTrainSetState();

    while (true) {
        status = Receive(&callee, &request, sizeof(request));
        if (status < 0) {
            error("Dispatcher: Error: Failed to receive meessage from Task %d with status %d",
                  callee, status);
            continue;
        }

        if ((node = getDispatcherNode(trains, request.tr)) && node->conductor != -1) {
            status = TRAIN_BUSY;
        }

        switch (request.type) {
            case TRM_ADD:
                if (getDispatcherNode(trains, request.tr)) {
                    status = INVALID_TRAIN_ID;
                    break;
                }
                status = -1;
                goto addnode;
                break;
            case TRM_ADD_AT:
                if (getDispatcherNode(trains, request.tr)) {
                    status = INVALID_TRAIN_ID;
                    break;
                }
                status = sensorToInt(request.arg0, request.arg1);
                if (status < 0 || status >= 80) {
                    status = INVALID_SENSOR_ID;
                    break;
                }
        addnode:
                if (CreateCourier != -1) {
                    Destroy(CreateCourier);
                    CreateCourier = -1;
                }
                if ((CreateCourier = addDispatcherTrain(request.tr, status)) == -1) {
                    status = OUT_OF_DISPATCHER_NODES;
                } else {
                    status = 0;
                }
                break;
            case TRM_CREATE_TRAIN:
                status = 0;
                if (addDispatcherNode(trains, request.arg0, request.tr) == NULL) {
                    status = OUT_OF_DISPATCHER_NODES;
                }
                CreateCourier = -1;
                break;
            case TRM_GOTO_STOP:
                if ((node = getDispatcherNode(trains, request.tr))) {
                    if (node->conductor == -1) {
                        status = TRAIN_HAS_NO_CONDUCTOR;
                        error("Dispatcher: Error: Train %u does not have a conductor", node->tr_number);
                    } else {
                        debug("Dispatcher: Removing Conductor for Train %u", node->tr_number);
                        Destroy(node->conductor);
                        node->conductor = -1;
                        status = 0;
                    }
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_MOVE:
                if (node != NULL) {
                    if (node->conductor == -1) {
                        debug("Dispatcher: Creating new conductor for train %u", node->tr_number);
                        node->conductor = Create(7, Conductor);
                    }
                    debug("Dispatcher: Received request to move train %u %u mm", node->tr_number, request.arg0);
                    status = Move(node->conductor, node->train, request.tr, request.arg0);
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_GOTO:
            case TRM_GOTO_AFTER:
                if (node != NULL) {
                    if (node->conductor == -1) {
                        debug("Dispatcher: Creating new conductor for train %u", node->tr_number);
                        node->conductor = Create(7, Conductor);
                    }
                    debug("Dispatcher: Received request to move train %u to %s", node->tr_number, (&track[request.arg0])->name);
                    status = GoTo(node->conductor, node->train, request.tr, &track[request.arg0], request.arg1);
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_AUX:
                if (node == NULL) {
                    status = INVALID_TRAIN_ID;
                } else if (status != TRAIN_BUSY) {
                    status = TrAuxiliary(node->train, request.arg0);
                }
                break;
            case TRM_RV:
                if (node == NULL) {
                    status = INVALID_TRAIN_ID;
                } else if (status != TRAIN_BUSY) {
                    status = TrReverse(node->train);
                }
                break;
            case TRM_GET_SPEED:
                if (node == NULL) {
                    status = INVALID_TRAIN_ID;
                } else if (status != TRAIN_BUSY) {
                    status = TrGetSpeed(node->train);
                }
                break;
            case TRM_SPEED:
                if (node == NULL) {
                    status = INVALID_TRAIN_ID;
                } else if (status != TRAIN_BUSY) {
                    status = TrSpeed(node->train, request.arg0);
                }
                break;
            case TRM_GET_TRACK_NODE:
                if (request.arg0 >= TRACK_MAX) {
                    status = 0;
                    break;
                }
                status = (int) &(track[request.arg0]);
                break;
            case TRM_RESERVE_TRACK_DIST:
                status = (int) reserveTrackDist(node->tr_number, (track_node**)request.arg0, request.arg1, (int*) request.arg2);
                break;
            case TRM_RESERVE_TRACK:
                status = (int)reserveTrack(node->tr_number, (track_node**)request.arg0, request.arg1);
                break;
            case TRM_RELEASE_TRACK:
                status = (int)releaseTrack(node->tr_number, (track_node**)request.arg0, request.arg1);
                break;
            default:
                error("Dispatcher: Unknown request of type %d from %u", request.type, callee);
                status = -1;
        }
        Reply(callee, &status, sizeof(status));
    }
    notice("Dispatcher: Exiting...");
    Exit();
}
