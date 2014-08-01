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
#include <random.h>
#include <util.h>

#define TRAIN_PRIORITY       6
#define MAX_NUM_OF_TRAINS    NUM_OF_TRAINS

DECLARE_CIRCULAR_BUFFER(int);

typedef enum {
    TRM_ADD = 77,
    TRM_READD,
    TRM_ADD_AT,
    TRM_GOTO_STOP,
    TRM_GOTO,
    TRM_GOTO_AFTER,
    TRM_GET_TRACK_NODE,
    TRM_RESERVE_TRACK,
    TRM_RESERVE_TRACK_DIST,
    TRM_RELEASE_TRACK,
    TRM_CREATE_TRAIN,
    TRM_REMOVE_TRAIN,
    TRM_MOVE,
    TRM_GET,
    TRM_TRACK_FREE
} DispatcherMessageType;

typedef struct DispatcherMessage {
    DispatcherMessageType type;
    uint32_t tr;
    int arg0;
    int arg1;
    int arg2;
} DispatcherMessage_t;

typedef struct {
    int train;
    int tr_number : 8;
    int conductor;
} DispatcherNode_t;


static uint32_t train_dispatcher_tid = -1;
static uint32_t num_of_trains = 0;
static void removeDispatcherNode(DispatcherNode_t*, DispatcherNode_t*);
static DispatcherNode_t *addDispatcherNode(DispatcherNode_t*, unsigned int, unsigned int);
static DispatcherNode_t *getDispatcherNode(DispatcherNode_t*, uint32_t);


static int SendDispatcherMessage(DispatcherMessageType type, uint32_t tr, int arg0, int arg1, int arg2) {
    DispatcherMessage_t msg;
    int status, bytes;
    uint32_t dispatcher;

    status = 0;
    dispatcher = WhoIs(TRAIN_DISPATCHER);
    msg.type = type;
    msg.tr = tr;
    msg.arg0 = arg0;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    if ((bytes = Send(dispatcher, &msg, sizeof(msg), &status, sizeof(status))) < 0) {
        error("Dispatcher: Error: Error in send to %u returned %d", dispatcher, bytes);
        return -1;
    }
    return status;
}


int DispatchTrackFree(uint32_t tr, int sensor) {
    return SendDispatcherMessage(TRM_TRACK_FREE, tr, sensor, 0, 0);
}


int DispatchGetTrainTid(uint32_t tr) {
    return SendDispatcherMessage(TRM_GET, tr, 0, 0, 0);
}


int DispatchRoute(uint32_t tr, uint32_t sensor, uint32_t dist) {
    return SendDispatcherMessage(TRM_GOTO, tr, sensor, dist, 0);
}


int DispatchAddTrain(uint32_t tr) {
    return SendDispatcherMessage(TRM_ADD, tr, 0, 0, 0);
}


int DispatchReAddTrain(uint32_t tr) {
    return SendDispatcherMessage(TRM_READD, tr, 0, 0, 0);
}


int DispatchAddTrainAt(uint32_t tr, char module, uint32_t id) {
    return SendDispatcherMessage(TRM_ADD_AT, tr, module, id, 0);
}


int DispatchStopRoute(uint32_t tr) {
    return SendDispatcherMessage(TRM_GOTO_STOP, tr, 0, 0, 0);
}


track_node *DispatchGetTrackNode(uint32_t id) {
    return (track_node*)SendDispatcherMessage(TRM_GET_TRACK_NODE, 0, id, 0, 0);
}


int DispatchTrainMove(uint32_t tr, uint32_t dist) {
    return SendDispatcherMessage(TRM_MOVE, tr, dist, 0, 0);
}


int DispatchReserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist) {
    return SendDispatcherMessage(TRM_RESERVE_TRACK_DIST, tr, (int) track, n, (int)dist);
}


int DispatchReserveTrack(uint32_t tr, track_node **track, uint32_t n) {
    return SendDispatcherMessage(TRM_RESERVE_TRACK, tr, (int) track, n, 0);
}


int DispatchReleaseTrack(uint32_t tr, track_node **track, uint32_t n) {
    return SendDispatcherMessage(TRM_RELEASE_TRACK, tr, (int)track, n, 0);
}


static int findSensorForTrain(uint32_t tr) {
    int sensorNum;
    trainSpeed(tr, 0);
    Delay(random_range(150, 200));
    trainSpeed(tr, 3);
    sensorNum = WaitAnySensor();
    trainSpeed(tr, 0);
    return sensorNum;
}


static void TrainCreateCourier() {
    DispatcherMessage_t req;
    int callee, tid, parent;
    int sensor, status, waiting;

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

    if ((tid = TrCreate(TRAIN_PRIORITY, req.tr, DispatchGetTrackNode(sensor))) >= 0) {
        req.type = TRM_CREATE_TRAIN;
        req.arg0 = tid;
        Send(parent, &req, sizeof(req), &status, sizeof(status));
        if (status < 0) {
            error("TrainCreateCourier: Error: Failed to fully create, destroying train with Tid %u", tid);
            Destroy(tid);
        } else {
            TrAuxiliary(tid, 16);
            if ((waiting = req.arg1) >= 0) {
                /* respond to the waiter with the Tid */
                Reply(waiting, &tid, sizeof(tid));
            }
        }
    } else {
        error("TrainCreateCourier: Error: Failed to create new Train task");
    }

    Exit();
}


static void TrainDeleteCourier() {
    DispatcherMessage_t req;
    int callee, tid, parent, status;

    parent = MyParentTid();
    Receive(&callee, &req, sizeof(req));
    if (callee != parent) {
        error("TrainDeleteCourier: Error: Received request from %u, expected %u", callee, parent);
        Exit();
    }
    Reply(callee, NULL, 0);
    tid = req.arg0;
    TrDelete(tid);
    req.type = TRM_REMOVE_TRAIN;
    Send(parent, &req, sizeof(req), &status, sizeof(status));
    Exit();
}


static DispatcherNode_t *addDispatcherNode(DispatcherNode_t *nodes, unsigned int tid, unsigned int tr) {
    unsigned int i;
    for (i = 0; i < MAX_NUM_OF_TRAINS; ++i) {
        if (nodes[i].train < 0) {
            nodes[i].train = tid;
            nodes[i].tr_number = tr;
            nodes[i].conductor = -1;
            break;
        }
    }
    if (i >= MAX_NUM_OF_TRAINS) {
        return NULL;
    }
    num_of_trains++;
    notice("Dispatcher: Created Train %u with tid %u, Total Number of Trains: %u",
           tr, nodes[i].train, num_of_trains);
    return &nodes[i];
}


static void removeDispatcherNode(DispatcherNode_t *trains, DispatcherNode_t *node) {
    DispatcherMessage_t req;
    unsigned int i, tid;
    for (i = 0; i < MAX_NUM_OF_TRAINS; ++i) {
        if (trains[i].train == node->train) {
            /* create the message to send to deletion courier */
            tid = Create(10, TrainDeleteCourier);
            req.type = TRM_REMOVE_TRAIN;
            req.tr = trains[i].tr_number;
            req.arg0 = trains[i].train;
            /* send off the message */
            Send(tid, &req, sizeof(req), NULL, 0);
            break;
        }
    }
}


static DispatcherNode_t *getDispatcherNode(DispatcherNode_t *nodes, uint32_t tr) {
    uint32_t i;
    for (i = 0; i < MAX_NUM_OF_TRAINS; ++i) {
        if (nodes[i].tr_number == tr) {
            return &nodes[i];
        }
    }
    return NULL;
}


static int addDispatcherTrain(unsigned int tr, int sensor, int callee) {
    DispatcherMessage_t req;
    unsigned int tid;

    if (num_of_trains < MAX_NUM_OF_TRAINS) {
        tid = Create(9, TrainCreateCourier);
        req.type = TRM_ADD;
        req.tr = tr;
        req.arg0 = sensor;
        req.arg1 = callee;
        Send(tid, &req, sizeof(req), NULL, 0);
        return tid;
    }
    return -1;
}


void Dispatcher() {
    unsigned int CreateCourier;
    DispatcherMessage_t request;
    track_node track[TRACK_MAX];
    CircularBuffer_int addQueue;
    int callee, status, waitingTrain, nextTrain, sensor;
    DispatcherNode_t *node, trains[MAX_NUM_OF_TRAINS] = {{0}};

    int i;
    for (i = 0; i < MAX_NUM_OF_TRAINS; ++i) {
        trains[i].train = -1;
    }

    init_track(track);
    initcb_int(&addQueue);
    RegisterAs(TRAIN_DISPATCHER);
    train_dispatcher_tid = MyTid();
    notice("Dispatcher: Tid %u", train_dispatcher_tid);
    num_of_trains = 0;
    CreateCourier = -1;
    waitingTrain = -1;
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
                if (node != NULL) {
                    removeDispatcherNode(trains, node);
                }
                sensor = -1;
                nextTrain = request.tr;
                goto addnode;
                break;
            case TRM_ADD_AT:
                if (node != NULL) {
                    removeDispatcherNode(trains, node);
                }
                sensor = sensorToInt(request.arg0, request.arg1);
                if (sensor < 0 || sensor >= 80) {
                    status = INVALID_SENSOR_ID;
                    break;
                }
                nextTrain = request.tr;
        addnode:
                if (nextTrain == waitingTrain) {
                    if (CreateCourier != -1) {
                        Destroy(CreateCourier);
                        CreateCourier = -1;
                    } else {
                        error("Dispatcher: Train %d is waiting but nothing is creating", nextTrain);
                        break;
                    }
                } else if (nextTrain != waitingTrain) {
                    if (waitingTrain >= 0) {
                        status = -1;
                        write_int(&addQueue, &nextTrain, 1);
                        write_int(&addQueue, &status, 1);
                        status = 0;
                        break;
                    }
                }
                waitingTrain = nextTrain;
                if ((CreateCourier = addDispatcherTrain(nextTrain, sensor, -1)) == -1) {
                    status = OUT_OF_DISPATCHER_NODES;
                } else {
                    status = 0;
                }
                break;
            case TRM_READD:
                if (node == NULL) {
                    error("Dispatcher: Called to re-add a train that was never added: Callee Tid %d", callee);
                    break;
                }
                nextTrain = request.tr;
        readd:
                if (nextTrain == waitingTrain) {
                    if (CreateCourier != -1) {
                        Destroy(CreateCourier);
                        CreateCourier = -1;
                    } else {
                        error("Dispatcher: Train %d is waiting but nothing is creating", nextTrain);
                        break;
                    }
                } else if (nextTrain != waitingTrain) {
                    if (waitingTrain >= 0) {
                        if (node->train == -1) {
                            error("Dispatcher: Callee %d re-adding a train %d that is currently being added or queued (queue length: %d)",
                                  callee, request.tr, length_int(&addQueue));
                            break;
                        }
                        /* queue the train to be added */
                        write_int(&addQueue, &nextTrain, 1);
                        write_int(&addQueue, &callee, 1);
                        continue;
                    }
                }
                node->train = -1;
                num_of_trains--;
                waitingTrain = nextTrain;
                sensor = -1;
                if ((CreateCourier = addDispatcherTrain(nextTrain, sensor, callee)) == -1) {
                    error("Dispatcher: Error: Out of dispatcher nodes in re-add.");
                    status = OUT_OF_DISPATCHER_NODES;
                } else {
                    continue;
                }
                break;
            case TRM_REMOVE_TRAIN:
                status = 0;
                notice("Dispatcher: Removing train %d with Tid %d", node->tr_number, node->train);
                if (node->conductor != -1) {
                    /* specifies whether or not to remove the conductor */
                    Destroy(node->conductor);
                }
                trains[request.arg1].conductor = -1;
                trains[request.arg1].tr_number = -1;
                trains[request.arg1].train = -1;
                num_of_trains--;
                break;
            case TRM_CREATE_TRAIN:
                status = 0;
                ASSERT(callee == CreateCourier, "Dispatcher: Something not the CreateCourier tried to create a train"
                       "Expected Tid %d, instead received called from Tid %d", CreateCourier, callee);
                if ((node = addDispatcherNode(trains, request.arg0, request.tr)) == NULL) {
                    error("Dispatcher: Error: Out of dispatcher nodes in train create.");
                    status = OUT_OF_DISPATCHER_NODES;
                    break;
                }
                CreateCourier = -1;
                waitingTrain = -1;
                nextTrain = -1;
                status = node->train;
                if (request.arg1 >= 0) {
                    /* add this as a conductor */
                    node->conductor = request.arg1;
                }

                if (length_int(&addQueue) > 0) {
                    Reply(callee, &status, sizeof(status));
                    read_int(&addQueue, &nextTrain, 1);
                    read_int(&addQueue, &callee, 1);
                    node = getDispatcherNode(trains, nextTrain);
                    sensor = -1;
                    if (node == NULL || callee == -1) {
                        goto addnode;
                    } else {
                        goto readd;
                    }
                }
                break;
            case TRM_GOTO_STOP:
                if ((node = getDispatcherNode(trains, request.tr))) {
                    if (node->conductor == -1) {
                        status = TRAIN_HAS_NO_CONDUCTOR;
                        error("Dispatcher: Error: Train %u does not have a conductor", node->tr_number);
                    } else {
                        notice("Dispatcher: Removing conductor with tid %d (by reply)", node->train);
                        status = 0;
                        Reply(node->conductor, &status, sizeof(status));
                        node->conductor = -1;
                        status = 0;
                    }
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_MOVE:
                if (node != NULL) {
                    if (node->conductor != -1) {
                        Destroy(node->conductor);
                    }
                    node->conductor = Create(7, Conductor);
                    status = Move(node->conductor, node->train, request.tr, request.arg0);
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_GOTO:
            case TRM_GOTO_AFTER:
                if (node != NULL) {
                    if (node->conductor != -1) {
                        notice("Dispatcher; Removing conductor tid %d for train %d", node->conductor, node->tr_number);
                        Destroy(node->conductor);
                    }
                    node->conductor = Create(7, Conductor);
                    status = GoTo(node->conductor, node->train, request.tr, &track[request.arg0], request.arg1);
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_GET:
                if (node == NULL) {
                    status = INVALID_TRAIN_ID;
                } else if (status != TRAIN_BUSY) {
                    status = node->train;
                }
                break;
            case TRM_GET_TRACK_NODE:
                if (request.arg0 >= TRACK_MAX) {
                    status = 0;
                    break;
                }
                status = (int)&(track[request.arg0]);
                break;
            case TRM_RESERVE_TRACK_DIST:
                status = reserveTrackDist(node->tr_number, (track_node**)request.arg0, request.arg1, (int*)request.arg2);
                break;
            case TRM_RESERVE_TRACK:
                status = reserveTrack(node->tr_number, (track_node**)request.arg0, request.arg1);
                break;
            case TRM_RELEASE_TRACK:
                status = releaseTrack(node->tr_number, (track_node**)request.arg0, request.arg1);
                break;
            case TRM_TRACK_FREE:
                status = 0;
                if (request.arg0 < TRACK_MAX) {
                    track_node *src;
                    src = &(track[request.arg0]);
                    if (src->reservedBy == RESERVED_BY_NOBODY || src->reverse->reservedBy == RESERVED_BY_NOBODY ||
                        src->reservedBy == request.tr || src->reverse->reservedBy == request.tr) {
                        status = 1;
                    }
                }
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
