#include <dispatcher.h>
#include <train_task.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <stdlib.h>
#include <clock.h>
#include <term.h>

static unsigned int train_dispatcher_tid = -1;

typedef struct {
    unsigned int train;
    unsigned int tr_number : 8;
    unsigned int conductor;
    unsigned int lastSensor;
} DispatcherNode_t;

typedef enum {
    CNTRL_GOTO = 88,
    CNTRL_STOP,
    CNTRL_ADD,
    CNTRL_ADD_AT,
    NUM_TRAIN_CNTRL_MESSAGES
} TrainControllerMessageTypes;


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


void Dispatcher() {
    DispatcherNode_t trains[NUM_OF_TRAINS];
    track_edge *edge;
    int callee, status;
    unsigned int num_of_trains, i;
    track_node track[TRACK_MAX];

    init_track(track);
    RegisterAs(TRAIN_CONTROLLER);
    train_dispatcher_tid = MyTid();
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
            case CNTRL_ADD:
                break;
            case CNTRL_ADD_AT:
                edge = getNearestEdge(request.arg1, request.arg2);
                if (edge == NULL) {
                    status = INVALID_SENSOR_ID;
                } else {
                    for (i = 0; i < num_of_trains; ++i) {
                        if (trains[i].tr_number == request.arg0) {
                            break;
                        }
                    }

                    if (i == num_of_trains && num_of_trains < NUM_OF_TRAINS) {
                        trains[i].train = TrCreate(6, request.arg0, edge);
                        if (tid >= 0) {
                            trains[i].tr_number = request.arg0;
                            /* can guarantee absolutely that 0 is not a valid ID */
                            trains[i].conductor = 0;
                            TrSpeed(trains[i].train, 0);
                            status = 1;
                            num_of_trains++;
                            notice("Dispatcher: Created Train %u with tid %u", trains[i].tr_number, trains[i].train);
                        } else {
                            status = INVALID_TRAIN_ID;
                        }
                    } else {
                        /* already on track, re-add it at a new position by sending message */
                        debug("Dispatcher: Train %u already on track, re-positioning", trains[i].tr_number);
                    } else {
                        status = OUT_OF_DISPATCHER_NODES;
                        error("Dispatcher: Error: Out of dispatcher nodes for new trains");
                    }
                }
                break;
            case CNTRL_STOP:
                for (i = 0; i < num_of_trains; ++i) {
                    if (trains[i].tr_number == request.arg0) {
                        break;
                    }
                }

                if (i < num_of_trains) {
                    if (trains[i].conductor != 0) {
                        Destroy(trains[i].conductor);
                        trains[i].conductor = 0;
                    } else {
                        error("Dispatcher: Error: Train %u does not have a conductor", i);
                        status = TRAIN_HAS_NO_CONDUCTOR;
                    }
                } else {
                    status = INVALID_TRAIN_ID;
                }
                break;
            case CNTRL_GOTO:
                break;
            default:
                status = -1;
        }
        Reply(callee, &status, sizeof(status));
    }
    notice("Dispatcher: Exiting...");
    Exit();
}

