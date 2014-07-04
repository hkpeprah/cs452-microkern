#include <server.h>
#include <syscall.h>
#include <clock.h>
#include <sensor_server.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>
#include <train_speed.h>

#define TIMEOUT_BUFFER 20

typedef struct __Train_t {
    unsigned int id : 16;
    unsigned int speed : 8;
    unsigned int aux : 8;
    track_edge *currentEdge;
    unsigned int edgeDistanceMM : 16;       // distance from src of edge, updated on speed change
    unsigned int lastDistUpdateTick;        // time of above update
    unsigned int microPerTick: 16;          // micrometer / clock tick speed
} Train_t;

typedef enum {
    TRM_INIT = 8372,        // sent on creation, pass tr number (a0)  and init location (a1)
    TRM_EXIT,               // no args, stop the train
    TRM_SENSOR_WAIT,        // sensor in arg0, time in arg1
    TRM_TIME_WAIT,          // arg1 ticks to wait, arg0 not used
    TRM_FREE_COURIER,       // optimistic courier free from time/sensor wait
    TRM_SPEED,              // speed in arg0
    TRM_AUX,                // auxiliary train function
    TRM_RV,                 // no args
    TRM_GET_LOCATION,       // track_edge ptr in arg0, dist (mm) in arg1
    TRM_GET_SPEED,          // no args
} TrainMessageType;

typedef struct TrainMessage {
    TrainMessageType type;
    int arg0;
    int arg1;
} TrainMessage_t;



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

int TrGetSpeed(int tid) {
    TrainMessage_t msg = {.type = TRM_GET_SPEED};
    int result = Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    if (result < 0) {
        return result;
    }
    return msg.arg0;
}

int TrGetLocation(unsigned int tid, track_edge **edge, unsigned int *edgeDistMM) {
    int result;

    TrainMessage_t msg = {.type = TRM_GET_LOCATION};
    result = Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    *edge = (track_edge*) msg.arg0;
    *edgeDistMM = msg.arg1;

    return result;
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
            printf("switch %d state: %c\n", sw->id, sw->state == DIR_STRAIGHT ? 's' : 'c');
            train->currentEdge = &(dest->edge[sw->state]);
            break;
        case NODE_NONE:
        default:
            error("BAD NODE TYPE");
    }
}

static inline void syncWithSensor(Train_t *train) {
    traverseNode(train);
    train->edgeDistanceMM = 0;
    train->lastDistUpdateTick = Time();
}

static inline void updateLocation(Train_t *train) {
    // TODO: account for acceleration
    unsigned int currentTick = Time();

    printf("updating location: prev tick: %d, currentTic: %d, prev dist: %d\n", train->lastDistUpdateTick, currentTick, train->edgeDistanceMM);

    if (currentTick == train->lastDistUpdateTick) {
        return;
    }

    train->edgeDistanceMM += (currentTick - train->lastDistUpdateTick) * train->microPerTick / 1000;
    train->lastDistUpdateTick = currentTick;

    printf("new distance: %d\n", train->edgeDistanceMM);

    while (train->edgeDistanceMM >= train->currentEdge->dist) {
        printf("edge dist %d, actual %d, traversing to %s\n", train->edgeDistanceMM, train->currentEdge->dist, train->currentEdge->dest->name);
        train->edgeDistanceMM -= train->currentEdge->dist;
        traverseNode(train);
    }
}


static inline void waitOnNextSensor(Train_t *train, int sensorCourier) {
    int result;
    TrainMessage_t msg;
    updateLocation(train);
    track_node *dest = train->currentEdge->dest;

    msg.arg1 = train->lastDistUpdateTick + ((train->currentEdge->dist - train->edgeDistanceMM) * 1000 / train->microPerTick) + TIMEOUT_BUFFER;

    if (dest->type == NODE_SENSOR) {
        msg.type = TRM_SENSOR_WAIT;
        msg.arg0 = dest->num;
        msg.arg1 = msg.arg1 + 200; // temporary - remove when acceleration is added
    } else {
        msg.type = TRM_TIME_WAIT;
    }

    printf("NEXT WAIT: %s at %d (current time %d) waiting for time %d\n", dest->name, msg.arg1, train->lastDistUpdateTick, msg.arg1 - train->lastDistUpdateTick);

    if ( (result = Reply(sensorCourier, &msg, sizeof(msg)) < 0) ) {
        error("TrainTask: waitOnNextSensor: Reply returned %d", result);
    }
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
                result = WaitWithTimeout(msg.arg0, msg.arg1);
                msg.arg0 = result;
                break;

            case TRM_TIME_WAIT:
                DelayUntil(msg.arg1);
                msg.arg0 = TIMER_TRIP;
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


static void TrainTask() {
    int sensorCourier;
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

                printf("courier returned!\n");
                if (msg.arg0 == SENSOR_TRIP) {
                    syncWithSensor(&train);
                }

                // if the train is driving, send courier out to wait for next sensor/time trip
                // this can be triggered either by an actual sensor trip or an invalidated
                // courier wait (ie. speed changes, train reverses)
                if (train.speed > 0) {
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
                break;

            case TRM_SPEED:
                printf("train speed setting to: %d\n", msg.arg0);
                // previously driving -> update location, get courier back
                // courier should send back and get picked up by TRM_SENSOR/TIME_WAIT
                updateLocation(&train);
                if (train.speed > 0) {
                    msg.type = TRM_FREE_COURIER;
                    result = Reply(sensorCourier, &msg, sizeof(msg));

                    if (result < 0) {
                        error("TrainTask: TRM_SPEED: bad reply result %d", result);
                    }
                }

                // update to new speed
                train.speed = msg.arg0;
                train.microPerTick = getTrainVelocity(train.id, train.speed);

                printf("Train Speed: %u micrometers/tick\r\n", train.microPerTick);
                printf("Location: %s + %u micrometers\r\n", train.currentEdge->src->name, train.edgeDistanceMM);

                // if previously stationary, use new data to wait
                if (courierReady && train.speed > 0) {
                    // deal with acceleration
                    courierReady = 0;
                    waitOnNextSensor(&train, sensorCourier);
                } else if (train.speed == 0) {
                    // deal with deceleration
                }

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

            case TRM_GET_SPEED:
                msg.arg0 = train.microPerTick;
                result = Reply(sender, &msg, sizeof(msg));
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
