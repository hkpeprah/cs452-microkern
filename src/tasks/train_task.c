#include <stdlib.h>
#include <server.h>
#include <syscall.h>
#include <clock.h>
#include <sensor_server.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>
#include <train_speed.h>
#include <calibration.h>
#include <types.h>

#define TIMEOUT_BUFFER 20

typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
} TransitionState_t;


typedef struct __Train_t {
    unsigned int id : 16;
    unsigned int speed : 8;
    unsigned int aux : 16;
    track_edge *currentEdge;
    unsigned int edgeDistance : 16;
    unsigned int lastUpdateTick;
    unsigned int microPerTick : 16;
    TransitionState_t *transition;
} Train_t;


typedef enum {
    TRM_INIT = 1337,
    TRM_EXIT,
    TRM_SENSOR_WAIT,
    TRM_TIME_WAIT,
    TRM_TIME_READY,
    TRM_SPEED,
    TRM_AUX,
    TRM_RV,
    TRM_GET_LOCATION,
    TRM_GET_SPEED,
    TRM_DIR
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

int TrGetSpeed(unsigned int tid) {
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
    *edge = (track_edge*)msg.arg0;
    *edgeDistMM = msg.arg1;

    return result;
}

int TrAuxiliary(unsigned int tid, unsigned int aux) {
    TrainMessage_t msg = {.type = TRM_AUX, .arg0 = aux};
    int status;
    if ((aux >= 16 && aux < 32) || aux == TRAIN_HORN_OFFSET) {
        Send(tid, &msg, sizeof(msg), &status, sizeof(status));
        return status;
    }
    return -1;
}


static void CalibrationSnapshot(Train_t *train) {
    CalibrationSnapshot_t snapshot;
    snapshot.tr = train->id;
    snapshot.sp = train->speed;
    snapshot.dist = train->edgeDistance;
    snapshot.landmark = train->currentEdge->src->name;
    snapshot.nextmark = train->currentEdge->dest->name;
    printTrainSnapshot(&snapshot);
}


int LookupTrain(unsigned int id) {
    char name[] = "TrainXX";
    name[5] = (id / 10) + '0';
    name[6] = (id % 10) + '0';
    return WhoIs(name);
}


static void TrainWatchDog() {
    TrainMessage_t request;
    int status, callee;
    unsigned int wait, train;

    wait = 0;
    train = MyParentTid();
    status = Receive(&callee, &request, sizeof(request));
    if (callee == train) {
        wait = request.arg0;
        status = Reply(callee, &request, sizeof(request));
        if (request.type == TRM_TIME_WAIT) {
            debug("WatchDog: Delaying for %u ticks", wait);
            if (Delay(wait) < 0) {
                error("WatchDog: Something went wrong with delay.");
            }
            request.type = TRM_TIME_WAIT;
            Send(train, &request, sizeof(request), &status, sizeof(status));
        } else {
            error("WatchDog: Bad message type %d from %d", request.type, train);
        }
    } else {
        error("WatchDog: Got message from something not the train.");
        Reply(callee, &request, sizeof(request));
    }
    Exit();
}


static void TrainReverseCourier() {
    TrainMessage_t request, message = {.type = TRM_DIR};
    int status, callee;

    Receive(&callee, &request, sizeof(request));
    status = 0;
    if (callee == MyParentTid()) {
        Reply(callee, &status, sizeof(status));
        TrSpeed(callee, 0);
        Delay(getTransitionTicks(request.arg1, request.arg0, 0));
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        Delay(10);
        TrSpeed(callee, request.arg0);
        Delay(getTransitionTicks(request.arg1, 0, request.arg0));
    } else {
        status = -1;
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}


static void TrainCourierTask() {
    /*
     * IMPORTANT IMPLEMENTATION NOTE: While the WatchDog is perfectly fine being killed by time,
     * this does not hold true for the courier task which may never wake up and thus must be killed.
     */
    int status, train;
    TrainMessage_t request;

    train = MyParentTid();
    request.type = TRM_SENSOR_WAIT;

    while (true) {
        if ((status = Send(train, &request, sizeof(request), &request, sizeof(request))) < 0) {
            notice("TrainCourierTask: Warning: Send returned %d to %d, Exiting....", status, train);
            break;
        }

        switch (request.type) {
            case TRM_SENSOR_WAIT:
                debug("TrainCourierTask: Waiting on Next Sensor");
                status = WaitOnSensorN(request.arg0);
                if (status == TIMER_TRIP) {
                    /* TrainTask told us to give up... */
                    request.type = TRM_SENSOR_WAIT;
                } else {
                    request.type = TRM_SENSOR_WAIT;
                    request.arg0 = SENSOR_TRIP;
                }
                break;
            case TRM_EXIT:
                Exit();
                break;
            default:
                error("TrainCourierTask: Error: Invalid message type %d", request.type);
                request.type = TRM_SENSOR_WAIT;
        }

    }
    Exit();
}


static void traverseNode(Train_t *train, track_node *dest) {
    Switch_t *swtch;
    train->lastUpdateTick = Time();
    switch (dest->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
        case NODE_EXIT:
            train->currentEdge = &(dest->edge[DIR_AHEAD]);
            train->edgeDistance = 0;
            break;
        case NODE_BRANCH:
            swtch = getSwitch(dest->num);
            train->currentEdge = &(dest->edge[swtch->state]);
            train->edgeDistance = 0;
            break;
        case NODE_NONE:
            break;
        default:
            error("UpdateLocation: Error: Bad node type %u", dest->type);
    }
}


static void updateLocation(Train_t *train) {
    unsigned int ticks, dist, transition_ticks;

    ticks = Time();
    if (ticks == train->lastUpdateTick) {
        CalibrationSnapshot(train);
        return;
    }

    /* account for acceleration/deceleration by considering
     * the transition state */
    dist = train->edgeDistance;
    if (train->transition->valid) {
        transition_ticks = getTransitionTicks(train->id, train->transition->start_speed, train->transition->dest_speed);
        if (transition_ticks <= (ticks - train->transition->time_issued)) {
            /* we've passed our transition period */
            train->transition->valid = false;
            dist += (ticks - transition_ticks - train->transition->time_issued) * train->microPerTick / 1000;
            dist += getStoppingDistance(train->id, train->transition->start_speed, train->transition->dest_speed);
        } else {
            /* otherwise we're still in it */
            transition_ticks = ticks - train->transition->time_issued;
            dist += getTransitionDistance(train->id, train->transition->start_speed, train->transition->dest_speed, transition_ticks);
        }
    } else {
        dist += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    while (dist >= train->currentEdge->dist) {
        /* update the location of the train by traversing nodes */
        dist -= train->currentEdge->dist;
        traverseNode(train, train->currentEdge->dest);
    }

    train->edgeDistance = dist;
    CalibrationSnapshot(train);
}


static unsigned int WaitOnNextTarget(Train_t *train, unsigned int SensorCourier, unsigned int WatchDog) {
    TrainMessage_t msg1, msg2;
    track_node *dest;
    int start_speed, dest_speed;
    unsigned int ticks, timeout, velocity, busy_status, distance;

    /* update the location of the train prior to computations */
    updateLocation(train);

    dest = train->currentEdge->dest;
    busy_status = 0;
    if (train->transition->valid) {
        start_speed = train->transition->start_speed;
        dest_speed = train->transition->dest_speed;
        /* compute distance travelled accelerating/decelerating, and the number
         * of ticks it takes to make that. */
        distance = getStoppingDistance(train->id, start_speed, dest_speed) * 1000;
        ticks = getTransitionTicks(train->id, start_speed, dest_speed);
        /* fix distance to be from the start speed to destination speed */
        distance *= ABS(start_speed - dest_speed);
        distance /= MAX(start_speed, dest_speed);
        velocity = distance / ticks;
    } else {
        velocity = train->microPerTick;
    }

    distance = (train->currentEdge->dist - train->edgeDistance);
    while (dest->type != NODE_SENSOR) {
        traverseNode(train, dest);
        dest = train->currentEdge->dest;
        distance += train->currentEdge->dist;
    }

    timeout = (distance * 1000) / velocity;
    debug("TrainTask: SensorCourier blocking on %s", dest->name);
    msg1.type = TRM_SENSOR_WAIT;
    msg1.arg0 = dest->num;
    Reply(SensorCourier, &msg1, sizeof(msg1));
    busy_status = dest->num;

    if (timeout > 0 && (train->speed > 0 || train->transition->valid)) {
        msg2.type = TRM_TIME_WAIT;
        msg2.arg0 = timeout;
        debug("TrainTask: Expected ETA: %u", Time() + timeout);
        Send(WatchDog, &msg2, sizeof(msg2), &msg2, sizeof(msg2));
    }

    return busy_status;
}


static void TrainTask() {
    TrainMessage_t request;
    Train_t train = {0};
    track_node *destination;
    char command[2], name[] = "TrainXX";
    short speed, courier_busy;
    int status, bytes, callee;
    unsigned int SensorCourier, ReverseCourier, WatchDog;
    TransitionState_t state;

    status = Receive(&callee, &request, sizeof(request));
    if (status < 0) {
        error("TrainTask: Error: Received %d from %d", status, callee);
        Exit();
    } else if (request.type != TRM_INIT) {
        error("TrainTask: Error: Received message not of form TRM_INIT: %d from %d", status, callee);
        Exit();
    }

    train.id = request.arg0;
    train.aux = 0;
    train.speed = 0;
    train.currentEdge = (track_edge*)request.arg1;
    train.lastUpdateTick = 0;
    train.microPerTick = 0;
    train.edgeDistance = 0;
    train.transition = &state;

    state.valid = false;

    name[5] = (train.id / 10) + '0';
    name[6] = (train.id % 10) + '0';
    ReverseCourier = 0;
    courier_busy = 0;
    WatchDog = 0;

    RegisterAs(name);
    SensorCourier = Create(4, TrainCourierTask);
    Reply(callee, NULL, 0);

    while (true) {
        if ((bytes = Receive(&callee, &request, sizeof(request))) < 0) {
            error("TrainTask: Error: Received %d from %d", bytes, callee);
            Reply(callee, &status, sizeof(status));
            continue;
        }

        status = 0;
        destination = train.currentEdge->dest;
        switch (request.type) {
            case TRM_SENSOR_WAIT:
                if (callee != SensorCourier) {
                    status = -1;
                    Reply(callee, &status, sizeof(status));
                    break;
                } else if (request.arg0 == SENSOR_TRIP) {
                    /* sensor was tripped by some train */
                    traverseNode(&train, train.currentEdge->dest);
                    courier_busy = 0;
                }

                /* sensor is waiting for something to wait on */
                if (WatchDog != 0) {
                    debug("TrainTask: Actual TTA: %u\r\nTrainTask: Sensor called, removing previous WatchDog.", Time());
                    Destroy(WatchDog);
                }
                WatchDog = Create(3, TrainWatchDog);
                courier_busy = WaitOnNextTarget(&train, SensorCourier, WatchDog);
                break;
            case TRM_TIME_WAIT:
                if (callee != WatchDog) {
                    error("TrainTask: Error: Call from something not the WatchDog");
                    status = -1;
                    Reply(callee, &status, sizeof(status));
                    break;
                } else {
                    /* previous sensor is no longer valid, figure out what
                       is going on with our movement. */
                    if (train.speed != 0) {
                        traverseNode(&train, train.currentEdge->dest);
                        updateLocation(&train);
                        /* remove the previous WatchDog by just unblocking it */
                        Reply(WatchDog, NULL, 0);
                        WatchDog = 0;
                        if (courier_busy > 0) {
                            FreeSensor(courier_busy);
                            courier_busy = 0;
                        } else if (courier_busy == 0) {
                            WatchDog = Create(3, TrainWatchDog);
                            courier_busy = WaitOnNextTarget(&train, SensorCourier, WatchDog);
                        }
                    }
                }
            case TRM_TIME_READY:
                break;
            case TRM_SPEED:
                speed = request.arg0;
                /* don't care if the speeds are the same */
                if (speed != train.speed) {
                    /* compensate for acceleration */
                    train.transition->valid = true;
                    train.transition->start_speed = train.speed;
                    train.transition->dest_speed = speed;
                    train.transition->time_issued = Time();
                    train.speed = speed;
                    train.microPerTick = getTrainVelocity(train.id, train.speed);
                    if (WatchDog != 0) {
                        Destroy(WatchDog);
                        WatchDog = 0;
                    }
                    if (courier_busy > 0) {
                        FreeSensor(courier_busy);
                        courier_busy = 0;
                    } else {
                        WatchDog = Create(3, TrainWatchDog);
                        courier_busy = WaitOnNextTarget(&train, SensorCourier, WatchDog);
                    }
                }

                command[0] = train.speed + train.aux;
                command[1] = train.id;
                trnputs(command, 2);
                updateLocation(&train);
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_AUX:
                if (train.aux == TRAIN_HORN_OFFSET && request.arg0 == train.aux) {
                    train.aux = TRAIN_HORN_OFF;
                } else {
                    train.aux = request.arg0;
                }
                command[0] = train.speed + train.aux;
                command[1] = train.id;
                trnputs(command, 2);
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_DIR:
                if (callee == ReverseCourier) {
                    train.currentEdge = train.currentEdge->reverse;
                    train.edgeDistance = train.currentEdge->dist - train.edgeDistance;
                    command[0] = TRAIN_AUX_REVERSE;
                    command[1] = train.id;
                    trnputs(command, 2);
                    ReverseCourier = 0;
                }
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_RV:
                ReverseCourier = Create(3, TrainReverseCourier);
                request.type = TRM_RV;
                request.arg0 = train.speed;
                request.arg1 = train.id;
                Send(ReverseCourier, &request, sizeof(request), &status, sizeof(status));
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_GET_LOCATION:
            case TRM_GET_SPEED:
                /* TODO: Write these */
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            default:
                error("TrainTask: Error: Bad request type %d from %d", request.type, callee);
        }
    }

    Exit();
}
