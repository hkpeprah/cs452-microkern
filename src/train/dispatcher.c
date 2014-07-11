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

static unsigned int train_dispatcher_tid = -1;
static unsigned int num_of_trains = 0;

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
    TRM_RESERVE_TRACK,
    TRM_RELEASE_TRACK
} DispatcherMessageType;

typedef struct DispatcherMessage {
    DispatcherMessageType type;
    unsigned int tr;
    int arg0;
    int arg1;
    int arg2;
} DispatcherMessage_t;

typedef struct {
    unsigned int train;
    unsigned int tr_number : 8;
    int conductor;
} DispatcherNode_t;


static int SendDispatcherMessage(int type, unsigned int tr, int arg0, int arg1) {
    DispatcherMessage_t msg;
    int status, bytes;
    unsigned int dispatcher;

    status = 0;
    dispatcher = WhoIs(TRAIN_DISPATCHER);
    switch (type) {
        case TRM_SPEED:
            if (arg0 > TRAIN_MAX_SPEED) {
                status = INVALID_SPEED;
            }
            break;
        case TRM_RV:
            break;
        case TRM_AUX:
            break;
    }

    msg.type = type;
    msg.tr = tr;
    msg.arg0 = arg0;
    msg.arg1 = arg1;
    if (status != 0) {
        return status;
    } else if ((bytes = Send(dispatcher, &msg, sizeof(msg), &status, sizeof(status))) < 0) {
        error("Dispatcher: Error: Error in send to %u returned %d", dispatcher, bytes);
        return -1;
    }
    return status;
}


int DispatchTrainAuxiliary(unsigned int tr, unsigned int aux) {
    return SendDispatcherMessage(TRM_AUX, tr, aux, 0);
}


int DispatchTrainSpeed(unsigned int tr, unsigned int speed) {
    return SendDispatcherMessage(TRM_SPEED, tr, speed, 0);
}


int DispatchRoute(unsigned int tr, unsigned int sensor) {
    return SendDispatcherMessage(TRM_GOTO, tr, sensor, 0);
}


int DispatchAddTrain(unsigned int tr) {
    return SendDispatcherMessage(TRM_ADD, tr, 0, 0);
}


int DispatchAddTrainAt(unsigned int tr, char module, unsigned int id) {
    return SendDispatcherMessage(TRM_ADD_AT, tr, module, id);
}


int DispatchTrainReverse(unsigned int tr) {
    return SendDispatcherMessage(TRM_RV, tr, 0, 0);
}


int DispatchStopRoute(unsigned int tr) {
    return SendDispatcherMessage(TRM_GOTO_STOP, tr, 0, 0);
}


static DispatcherNode_t *addDispatcherNode(DispatcherNode_t *nodes, unsigned int tr, track_node *start_node) {
    unsigned int i;

    if (num_of_trains < NUM_OF_TRAINS) {
        i = num_of_trains;
        if ((nodes[i].train = TrCreate(6, tr, start_node)) >= 0) {
            notice("Dispatcher: Created Train %u with tid %u", tr, nodes[i].train);
            nodes[i].tr_number = tr;
            nodes[i].conductor = -1;
            num_of_trains++;
            return &nodes[i];
        } else {
            error("Dispatcher: Error: Failed to create new Train task");
            return NULL;
        }
    }
    return NULL;
}


static DispatcherNode_t *getDispatcherNode(DispatcherNode_t *nodes, unsigned int tr) {
    unsigned int i;
    for (i = 0; i < num_of_trains; ++i) {
        if (nodes[i].tr_number == tr) {
            return &nodes[i];
        }
    }
    return NULL;
}


static int sensorAttribution(unsigned int tr) {
    int sensorNum;
    trainSpeed(tr, 3);
    sensorNum = WaitAnySensor();
    trainSpeed(tr, 0);
    return sensorNum;
}


void Dispatcher() {
    int callee, status;
    DispatcherMessage_t request;
    track_node track[TRACK_MAX];
    DispatcherNode_t trains[NUM_OF_TRAINS], *node;

    init_track(track);
    RegisterAs(TRAIN_DISPATCHER);
    train_dispatcher_tid = MyTid();
    notice("Dispatcher: Tid %u", train_dispatcher_tid);
    num_of_trains = 0;
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
                status = sensorAttribution(request.tr);
                if (addDispatcherNode(trains, request.tr, &track[status]) != NULL) {
                    status = 0;
                } else {
                    status = OUT_OF_DISPATCHER_NODES;
                }
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
                status = 0;
                if (addDispatcherNode(trains, request.tr, &track[status]) == NULL) {
                    status = OUT_OF_DISPATCHER_NODES;
                }
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
            case TRM_GOTO:
            case TRM_GOTO_AFTER:
                if ((node = getDispatcherNode(trains, request.tr))) {
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
            case TRM_RESERVE_TRACK:
                break;
            case TRM_RELEASE_TRACK:
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

