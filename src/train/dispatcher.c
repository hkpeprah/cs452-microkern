#include <dispatcher.h>
#include <train_task.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <stdlib.h>
#include <clock.h>
#include <term.h>
#include <train.h>

static unsigned int train_dispatcher_tid = -1;
static unsigned int num_of_trains = 0;

typedef struct {
    unsigned int train;
    unsigned int tr_number : 8;
    unsigned int lastSensor;
    int conductor;
} DispatcherNode_t;


int CreateDispatcherMessage(TrainMessage_t *msg, int type, unsigned int tr, int arg0, int arg1) {
    int status, bytes;
    unsigned int dispatcher;

    status = 0;
    dispatcher = WhoIs(TRAIN_DISPATCHER);

    switch (type) {
        case TRM_SPEED:
            if (arg1 <= TRAIN_MAX_SPEED) {
                status = INVALID_SPEED;
            }
            break;
        case TRM_RV:
            break;
        case TRM_AUX:
            break;
    }

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


static bool isValidSensorId(unsigned int id) {
    return (id < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT ? 1 : 0);
}


static track_edge *getNearestEdgeId(unsigned int id, track_node *track) {
    if (isValidSensorId(id)) {
        /* TODO: Might be curved, eh ? */
        return &track[id].edge[DIR_AHEAD];
    }
    return NULL;
}


static track_edge *getNearestEdge(char module, unsigned int id, track_node *track) {
    int sensor;
    sensor = sensorToInt(module, id);
    if (sensor < 0) {
        return NULL;
    }
    return getNearestEdgeId(sensor, track);
}


static DispatcherNode_t *addDispatcherNode(DispatcherNode_t *nodes, unsigned int tr) {
    unsigned int i;

    if (num_of_trains < NUM_OF_TRAINS) {
        i = num_of_trains;
        if ((nodes[i].train = TrCreate(6, tr)) >= 0) {
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


void Dispatcher() {
    TrainMessage_t request;
    DispatcherNode_t trains[NUM_OF_TRAINS], *node;
    track_edge *edge;
    int callee, status;
    track_node track[TRACK_MAX];

    init_track(track);
    RegisterAs(TRAIN_DISPATCHER);
    train_dispatcher_tid = MyTid();
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
                /* TODO: sensor attribution */

                break;
            case TRM_ADD_AT:
                if (edge || (edge = getNearestEdge(request.arg0, request.arg1, track))) {
                    node = getDispatcherNode(trains, request.tr);
                    if (node == NULL) {
                        if ((node = addDispatcherNode(trains, request.tr))) {
                            status = 1;
                        } else {
                            status = OUT_OF_DISPATCHER_NODES;
                        }
                    } else {
                        /* Node is already on track, wrapzone it */
                        debug("Dispatcher: Train %u already on track, re-positioning", node->tr_number);
                    }
                } else {
                    status = INVALID_SENSOR_ID;
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
            case TRM_AUX:
            case TRM_RV:
            case TRM_GET_SPEED:
            case TRM_GET_LOCATION:
            case TRM_SPEED:
                if ((node = getDispatcherNode(trains, request.tr)) && node->conductor == -1) {
                    Send(node->train, &request, sizeof(request), &status, sizeof(status));
                } else if (node->conductor != -1) {
                    status = TRAIN_BUSY;
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            default:
                status = -1;
        }
        Reply(callee, &status, sizeof(status));
    }
    notice("Dispatcher: Exiting...");
    Exit();
}

