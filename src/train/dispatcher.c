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

static unsigned int train_dispatcher_tid = -1;
static unsigned int num_of_trains = 0;

typedef struct {
    unsigned int train;
    unsigned int tr_number : 8;
    int conductor;
} DispatcherNode_t;


int SendDispatcherMessage(TrainMessage_t *msg, int type, unsigned int tr, int arg0, int arg1) {
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

    msg->type = type;
    msg->tr = tr;
    msg->arg0 = arg0;
    msg->arg1 = arg1;
    if (status != 0) {
        return status;
    } else if ((bytes = Send(dispatcher, msg, sizeof(*msg), &status, sizeof(status))) < 0) {
        error("Dispatcher: Error: Error in send to %u returned %d", dispatcher, bytes);
        return -1;
    }
    return status;
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
    TrainMessage_t request;
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

        switch (request.type) {
            case TRM_ADD:
                status = sensorAttribution(request.tr);
                if (addDispatcherNode(trains, request.tr, &track[status]) != NULL) {
                    status = 0;
                } else {
                    status = OUT_OF_DISPATCHER_NODES;
                }
                break;
            case TRM_ADD_AT:
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
            case TRM_STOP:
                if ((node = getDispatcherNode(trains, request.tr))) {
                    if (node->conductor == -1) {
                        error("Dispatcher: Error: Train %u does not have a condcutor", node->conductor);
                    } else {
                        Destroy(node->conductor);
                        node->conductor = -1;
                    }
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case TRM_GOTO:
            case TRM_GOTO_AFTER:
                break;
            case TRM_AUX:
            case TRM_RV:
            case TRM_GET_SPEED:
            case TRM_GET_LOCATION:
            case TRM_SPEED:
                if ((node = getDispatcherNode(trains, request.tr)) != NULL) {
                    if (node->conductor == -1) {
                        Send(node->train, &request, sizeof(request), &status, sizeof(status));
                    } else {
                        status = TRAIN_BUSY;
                    }
                } else {
                    status = INVALID_TRAIN_ID;
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

