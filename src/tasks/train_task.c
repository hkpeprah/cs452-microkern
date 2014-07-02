#include <syscall.h>
#include <clock.h>
#include <controller.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>
#include <train_speed.h>

#define TRAIN_AUX_REVERSE   15

#define WAIT_TIME(microPerTick, dist) ((dist * 1000 / microPerTick) + 10)

static void TrainTask();

int TrCreate(int priority, int tr, track_edge *start) {
    int result;
    int trainTask = Create(priority, TrainTask);

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

int TrSpeed(int tid, int speed) {
    TrainMessage_t msg = {.type = TRM_SPEED, .arg0 = speed};
    return Send(tid, &msg, sizeof(msg), NULL, 0);
}

int TrReverse(int tid) {
    TrainMessage_t msg = {.type = TRM_RV};
    return Send(tid, &msg, sizeof(msg), NULL, 0);
}

int TrGetLocation(int tid, TrainMessage_t *msg) {
    msg->type = TRM_GET_LOCATION;
    return Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
}


static void TrainCourierTask() {
    int boss = MyParentTid();
    TrainMessage_t msg;
    msg.type = TRM_SENSOR_WAIT;

    for(;;) {
        Send(boss, &msg, sizeof(msg), &msg, sizeof(msg));

        switch (msg.type) {
            case TRM_SENSOR_WAIT:
                // wait on sensor with delay
                break;

            case TRM_TIME_WAIT:
                Delay(msg.arg1);
                break;

            case TRM_FREE_COURIER:
                break;

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

    train->edgeDistanceMM = 0;
    train->lastDistUpdateTick = Time();
}

static inline void updateLocation(Train_t *train) {
    unsigned int currentTick = Time();
    train->edgeDistanceMM += (currentTick - train->lastDistUpdateTick) * train->microPerTick / 1000;
    train->lastDistUpdateTick = currentTick;
}

static void TrainTask() {
    int sensorCourier;
    bool validWait = 0;
    int result, sender;
    char cmdbuf[2];
    TrainMessage_t msg;

    Train_t train = {0};
    track_node *dest;

    result = Receive(&sender, &msg, sizeof(msg));

    if (result < 0 || msg.type != TRM_INIT) {
        error("TrainTask: Receive error: %d or incorrect msg type (not TRM_INIT): %d", result, msg.type);
    }

    train.id = msg.arg0;
    train.currentEdge = (track_edge*) msg.arg1;
    Reply(sender, NULL, 0);

    // TODO: are these the right priorities?
    sensorCourier = Create(5, TrainCourierTask);

    for(;;) {
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

                    traverseNode(&train);
                }

                // in all cases, send courier out to wait for next sensor/time trip
                // this can be triggered either by an actual sensor trip or an invalidated
                // courier wait (ie. speed changes, train reverses)
                dest = train.currentEdge->dest;
                if (dest->type == NODE_SENSOR) {
                    msg.type = TRM_SENSOR_WAIT;
                    msg.arg0 = dest->num;
                } else {
                    msg.type = TRM_TIME_WAIT;
                }
                msg.arg1 = WAIT_TIME(train.microPerTick, train.currentEdge->dist);
                result = Reply(sensorCourier, &msg, sizeof(msg));
                validWait = 1;
                break;

            case TRM_SPEED:
                // update location
                // TODO: account for acceleration
                updateLocation(&train);
                train.microPerTick = getTrainVelocity(train.id, train.speed);

                // send bytes
                train.speed = msg.arg0;
                cmdbuf[0] = train.speed;
                cmdbuf[1] = train.id;
                trnputs(cmdbuf, 2);

                // free up courier, invalidate wait
                if (validWait) {
                    validWait = 0;
                    result = Reply(sensorCourier, NULL, 0);

                    if (result < 0) {
                        error("TrainTask: TRM_SPEED: bad reply result %d", result);
                    }
                }

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
                updateLocation(&train);
                msg.arg0 = (int) train.currentEdge;
                msg.arg1 = train.edgeDistanceMM;
                result = Reply(sender, &msg, sizeof(msg));
                break;

            default:
                error("TrainTask %d, incorrect msg type %d", train.id, msg.type);
        }

        if (result < 0) {
            error("TrainTask: request: %d, got bad reply result %d", msg.type, result);
        }
    }
}
