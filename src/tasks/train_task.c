#include <server.h>
#include <syscall.h>
#include <clock.h>
#include <controller.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>
#include <train_speed.h>

#define WAIT_TIME(microPerTick, dist) ((dist * 1000 / microPerTick))


typedef struct __Train_t {
    unsigned int id : 16;
    unsigned int speed : 8;
    unsigned int aux : 8;
    track_edge *currentEdge;
    unsigned int edgeDistanceMM : 16;       // distance from src of edge, updated on speed change
    unsigned int lastDistUpdateTick;        // time of above update
    unsigned int microPerTick: 16;          // micrometer / clock tick speed
} Train_t;


static void TrainTask();


int TrCreate(int priority, int tr, track_edge *start) {
    int result, trainTask;

    if (!isValidTrainId(tr)) {
        return -1;
    }

    trainTask = Create(priority, TrainTask);
    if (trainTask < 0) {
        return trainTask;
    }

    TrainMessage_t msg = {.type = TRM_INIT, .arg0 = tr, .arg1 = (int) start};
    result = Send(trainTask, &msg, sizeof(msg), NULL, 0);
    if (result < 0) {
        return result;
    }

    return trainTask;
}


int TrSpeed(unsigned int tid, unsigned int speed) {
    TrainMessage_t msg = {.type = TRM_SPEED, .arg0 = speed};
    int status;

    if (speed <= TRAIN_MAX_SPEED) {
        Send(tid, &msg, sizeof(msg), &status, sizeof(status));
        return status;
    }
    return -1;
}


int TrReverse(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_RV};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrGetLocation(unsigned int tid, TrainMessage_t *msg) {
    msg->type = TRM_GET_LOCATION;
    return Send(tid, msg, sizeof(TrainMessage_t), msg, sizeof(TrainMessage_t));
}


int TrAuxiliary(unsigned int tid, unsigned int aux) {
    TrainMessage_t msg = {.type = TRM_AUX, .arg0 = aux};
    int status;
    if (aux >= 16 && aux < 32) {
        Send(tid, &msg, sizeof(msg), &status, sizeof(status));
        return status;
    }
    return -1;
}


static void TrainCourierTask() {
    int result;
    int boss = MyParentTid();
    TrainMessage_t msg;
    msg.type = TRM_SENSOR_WAIT;

    for(;;) {
        result = Send(boss, &msg, sizeof(msg), &msg, sizeof(msg));

        if (result < 0) {
            debug("TrainCourierTask: Warning: send result %d, exiting", result);
            return;
        }

        switch (msg.type) {
            case TRM_SENSOR_WAIT:
                // wait on sensor with delay
                WaitOnSensorN(msg.arg0);
                break;

            case TRM_TIME_WAIT:
                Delay(msg.arg1);
                break;

            case TRM_FREE_COURIER:
                break;

            case TRM_EXIT:
                return;

            default:
                error("TrainCourierTask: Invalid message type %d", msg.type);
        }
    }
}


static inline void traverseNode(Train_t *train) {
    Switch_t *sw;
    track_node *dest = train->currentEdge->dest;

    switch (dest->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
        case NODE_EXIT:
            train->currentEdge = &(dest->edge[DIR_AHEAD]);
            break;
        case NODE_BRANCH:
            sw = getSwitch(dest->num);
            train->currentEdge = &(dest->edge[sw->state]);
            break;
        case NODE_NONE:
        default:
            error("BAD NODE TYPE");
    }
}


static inline void updateLocation(Train_t *train) {
    // TODO: account for acceleration
    unsigned int currentTick;

    currentTick = Time();
    train->edgeDistanceMM += (currentTick - train->lastDistUpdateTick) * train->microPerTick / 1000;
    train->lastDistUpdateTick = currentTick;

    if (train->edgeDistanceMM >= train->currentEdge->dist) {
        debug("edge dist %d, actual %d, traversing", train->edgeDistanceMM, train->currentEdge->dist);
        train->edgeDistanceMM -= train->currentEdge->dist;
        traverseNode(train);
    }
}


static inline void waitOnNextSensor(Train_t *train, int sensorCourier) {
    int result;
    TrainMessage_t msg;
    track_node *dest = train->currentEdge->dest;

    if (dest->type == NODE_SENSOR) {
        msg.type = TRM_SENSOR_WAIT;
        msg.arg0 = dest->num;
    } else {
        msg.type = TRM_TIME_WAIT;
    }

    msg.arg1 = WAIT_TIME(train->microPerTick, train->currentEdge->dist - train->edgeDistanceMM);
    debug("NEXT WAIT: %s for %d", dest->name, msg.arg1);
    if ( (result = Reply(sensorCourier, &msg, sizeof(msg)) < 0) ) {
        error("TrainTask: waitOnNextSensor: Reply returned %d", result);
    }
}


static void TrainTask() {
    int sensorCourier;
    bool validWait = 0;
    bool courierReady = 0;
    bool running = 1;
    int result, sender;
    char cmdbuf[2];
    TrainMessage_t msg;
    Train_t train = {0};
    track_node *dest;
    char name[] = "TrainXX";

    result = Receive(&sender, &msg, sizeof(msg));
    if (result < 0 || msg.type != TRM_INIT) {
        error("TrainTask: Receive error: %d or incorrect msg type (not TRM_INIT): %d", result, msg.type);
    }

    train.id = msg.arg0;
    train.aux = 0;
    train.speed = 0;
    train.currentEdge = (track_edge*)msg.arg1;
    train.lastDistUpdateTick = 0;
    train.microPerTick = 0;
    name[5] = (train.id / 10) + '0';
    name[6] = (train.id % 10) + '0';
    RegisterAs(name);
    Reply(sender, NULL, 0);

    // TODO: are these the right priorities?
    sensorCourier = Create(1, TrainCourierTask);
    while (running) {
        result = Receive(&sender, &msg, sizeof(msg));
        if (result < 0) {
            error("TrainTask: Receive error: %d", result);
        }

        dest = train.currentEdge->dest;
        switch (msg.type) {
            case TRM_TIME_WAIT:
                // TODO: something about time offset? also fall through here
            case TRM_SENSOR_WAIT:
                if (sender != sensorCourier) {
                    // bad msg, discard
                    Reply(sender, NULL, 0);
                }
                if (validWait) {
                    // sensor or time tripped, update position and edge
                    if (dest->num != msg.arg0) {
                        error("Expected dest node %d but received %d", dest->num, msg.arg0);
                    }

                    updateLocation(&train);
                }

                // if the train is driving, send courier out to wait for next sensor/time trip
                // this can be triggered either by an actual sensor trip or an invalidated
                // courier wait (ie. speed changes, train reverses)
                if (train.speed > 0) {
                    validWait = 1;
                    courierReady = 0;
                    waitOnNextSensor(&train, sensorCourier);
                } else {
                    courierReady = 1;
                }

                break;

            case TRM_AUX:
                // toggles an auxiliary function, don't have to worry about speed/location stuff
                train.aux = msg.arg0;
                cmdbuf[0] = train.speed + train.aux;
                cmdbuf[1] = train.id;
                trnputs(cmdbuf, 2);

                result = Reply(sender, NULL, 0);
                break;

            case TRM_SPEED:
                // previously driving -> update location, get courier back
                // courier should send back and get picked up by TRM_SENSOR/TIME_WAIT
                if (train.speed > 0) {
                    updateLocation(&train);
                    validWait = 0;
                    msg.type = TRM_FREE_COURIER;
                    result = Reply(sensorCourier, &msg, sizeof(msg));

                    if (result < 0) {
                        error("TrainTask: TRM_SPEED: bad reply result %d", result);
                    }
                }

                // update to new speed
                train.speed = msg.arg0;
                train.microPerTick = getTrainVelocity(train.id, train.speed);

                // if previously stationary, use new data to wait
                if (courierReady && train.speed > 0) {
                    validWait = 1;
                    courierReady = 0;
                    waitOnNextSensor(&train, sensorCourier);
                }

                debug("Train Speed: %u micrometers/tick", train.microPerTick);
                debug("Location: %s + %u micrometers", train.currentEdge->src->name, train.edgeDistanceMM);

                // send bytes
                cmdbuf[0] = train.speed + train.aux;
                cmdbuf[1] = train.id;
                trnputs(cmdbuf, 2);

                result = Reply(sender, NULL, 0);
                break;

            case TRM_RV:
                if (train.speed != 0) {
                    error("TrainTask: RV when speed is not 0");
                }
                train.currentEdge = train.currentEdge->reverse;
                train.edgeDistanceMM = train.currentEdge->dist - train.edgeDistanceMM;

                cmdbuf[0] = TRAIN_AUX_REVERSE;
                cmdbuf[1] = train.id;
                trnputs(cmdbuf, 2);

                result = Reply(sender, NULL, 0);
                break;

            case TRM_GET_LOCATION:
                if (train.speed > 0) {
                    updateLocation(&train);
                }
                msg.arg0 = (int) train.currentEdge;
                msg.arg1 = train.edgeDistanceMM;
                result = Reply(sender, &msg, sizeof(msg));
                break;

            case TRM_EXIT:
                running = 0;
                cmdbuf[0] = 0;
                cmdbuf[1] = train.id;
                trnputs(cmdbuf, 2);
                Reply(sender, NULL, 0);
                do {
                    debug("Freeing child courier");
                    result = Reply(sensorCourier, &msg, sizeof(msg));
                    Delay(10);
                } while (result != TASK_DOES_NOT_EXIST);
                break;

            default:
                error("TrainTask %d, incorrect msg type %d", train.id, msg.type);
        }

        if (result < 0) {
            error("TrainTask: request: %d, got bad reply result %d", msg.type, result);
        }
    }
}


int LookupTrain(unsigned int id) {
    char name[] = "TrainXX";
    name[5] = (id / 10) + '0';
    name[6] = (id % 10) + '0';
    return WhoIs(name);
}
