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
#include <path.h>
#include <random.h>
#include <dispatcher.h>

typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
    unsigned int stopping_distance;
} TransitionState_t;

typedef struct __Train_t {
    unsigned int id : 16;
    unsigned int speed : 8;
    unsigned int aux : 16;
    track_node *lastSensor;
    track_node *nextSensor;
    unsigned int edgeDistance : 16;
    unsigned int lastUpdateTick;
    unsigned int microPerTick : 16;
    unsigned int distToNextSensor;
    TransitionState_t *transition;
} Train_t;

static void TrainTask();


int TrCreate(int priority, int tr, track_node *start) {
    TrainMessage_t msg;
    int result, trainTask;

    if (!isValidTrainId(tr)) {
        return -1;
    }

    trainSpeed(tr, 0);
    msg.type = TRM_INIT;
    msg.tr = tr;
    msg.arg0 = (int)start;
    trainTask = Create(priority, TrainTask);
    if (trainTask < 0) {
        return trainTask;
    }

    result = Send(trainTask, &msg, sizeof(msg), NULL, 0);
    if (result < 0) {
        return result;
    }

    return trainTask;
}


int TrSpeed(unsigned int tid, unsigned int speed) {
    TrainMessage_t msg = {.type = TRM_SPEED, .arg0 = speed};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return 1;
}


int TrReverse(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_RV};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrGetSpeed(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_GET_SPEED};
    int status, speed;

    if ((status = Send(tid, &msg, sizeof(speed), &speed, sizeof(speed))) < 0) {
        return status;
    }
    return speed;
}


int TrAuxiliary(unsigned int tid, unsigned int aux) {
    TrainMessage_t msg = {.type = TRM_AUX, .arg0 = aux};
    int status, bytes;

    if ((bytes = Send(tid, &msg, sizeof(msg), &status, sizeof(status))) < 0) {
        return bytes;
    }
    return status;
}


track_node *TrGetLocation(unsigned int tid, unsigned int *distance) {
    TrainMessage_t msg = {.type = TRM_GET_LOCATION};

    Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    *distance = msg.arg1;

    return (track_node*)msg.arg0;
}


static void CalibrationSnapshot(Train_t *train) {
    CalibrationSnapshot_t snapshot;
    snapshot.tr = train->id;
    snapshot.sp = train->speed;
    snapshot.dist = train->edgeDistance;
    snapshot.landmark = train->lastSensor->name;
    snapshot.nextmark = train->nextSensor->name;
    printTrainSnapshot(&snapshot);
}


static void TrainTimer() {
    TrainMessage_t msg;
    int parent;

    parent = MyParentTid();
    msg.type = TRM_GET_LOCATION;
    while (true) {
        Delay(5);
        Send(parent, &msg, sizeof(msg), &msg, sizeof(msg));
    }
    Exit();
}


static track_edge *getNextEdge(track_node *node) {
    Switch_t *swtch;
    switch (node->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
            return &(node->edge[DIR_AHEAD]);
        case NODE_BRANCH:
            swtch = getSwitch(node->num);
            return &(node->edge[swtch->state]);
        case NODE_EXIT:
            return NULL;
        case NODE_NONE:
        default:
            error("getNextEdge: Error: Bad node type %u", node->type);
            return NULL;
    }
}


static void updateNextSensor(Train_t *train) {
    track_node *node = train->lastSensor;
    track_edge *edge = getNextEdge(node);

    train->distToNextSensor = 0;

    do {
        train->distToNextSensor += edge->dist;
        node = edge->dest;
        edge = getNextEdge(node);
    } while (node && node->type != NODE_SENSOR);

    train->nextSensor = node;
}

static void sensorTrip(Train_t *train, track_node *sensor) {
    if (sensor->type != NODE_SENSOR) {
        error("sensorTrip called with non-sensor node %s", sensor->name);
    }

    train->lastUpdateTick = Time();
    if (train->transition->valid
            && train->transition->stopping_distance >= train->distToNextSensor - train->edgeDistance) {
        train->transition->stopping_distance -= (train->distToNextSensor - train->edgeDistance);
    }
    train->edgeDistance = 0;
    train->lastSensor = sensor;

    updateNextSensor(train);
}


static void updateLocation(Train_t *train) {
    int ticks;
    unsigned int start_speed, stop_speed;

    ticks = Time();
    if (train->transition->valid) {
        start_speed = train->transition->start_speed;
        stop_speed = train->transition->dest_speed;
        if (ticks - train->transition->time_issued >= getTransitionTicks(train->id, start_speed, stop_speed)) {
            train->transition->valid = false;
            train->microPerTick = getTrainVelocity(train->id, train->transition->dest_speed);
            train->edgeDistance = train->transition->stopping_distance;
            train->edgeDistance += (ticks - train->transition->time_issued - getTransitionTicks(train->id, start_speed, stop_speed)) * train->microPerTick;
        }
    } else {
        train->edgeDistance += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }
    train->lastUpdateTick = ticks;
    CalibrationSnapshot(train);
}


static void SensorCourierTask() {
    TrainMessage_t request;
    int status, callee;
    unsigned int wait, train;

    wait = 0;
    train = MyParentTid();
    status = Receive(&callee, &request, sizeof(request));
    if (callee != train) {
        error("SensorCourier: incorrect rcv from %d", callee);
        Reply(callee, &request, sizeof(request));
        return;
    }

    wait = request.arg0;
    status = Reply(callee, &request, sizeof(request));
    if (request.type != TRM_SENSOR_WAIT) {
        error("SensorCourier: Bad message type %d from %d", request.type, train);
        return;
    }

    // debug("SensorCourier: Waiting on sensor %d", wait);
    if ((status = WaitOnSensorN(wait)) < 0) {
        error("SensorCourier: result from WaitOnSensorN: %d", status);
    }
    debug("SensorCourier: Tripped sensor %d", wait);
    Send(train, NULL, 0, NULL, 0);
}


static void TrainReverseCourier() {
    int callee, status;
    TrainMessage_t request, message = {.type = TRM_DIR};

    Receive(&callee, &request, sizeof(request));
    status = 0;
    if (callee == MyParentTid()) {
        debug("ReverseCourier: Stopping train tid %u with starting speed %u", callee, request.arg0);
        Reply(callee, &status, sizeof(status));
        message.type = TRM_SPEED;
        message.arg0 = 0;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        Delay(getTransitionTicks(request.tr, request.arg0, 0));
        debug("ReverseCourier: Reversing train tid %u", callee);
        message.type = TRM_DIR;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        Delay(20);
        debug("ReverseCourier: Sending to speed up train tid %u to %u", callee, request.arg0);
        message.type = TRM_SPEED;
        message.arg0 = request.arg0;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
    } else {
        status = -1;
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}


static void WaitOnNextTarget(Train_t *train, int *SensorCourier, int *waitingSensor) {
    TrainMessage_t msg1;

    if (*waitingSensor >= 0) {
        FreeSensor(*waitingSensor);
    }

    if (*SensorCourier >= 0) {
        Destroy(*SensorCourier);
        *SensorCourier = -1;
    }

    // potentially need to call updateNextSensor here? only if track state changed between
    // last sensor trip and this

    *waitingSensor = train->nextSensor->num;
    *SensorCourier = Create(4, SensorCourierTask);
    msg1.type = TRM_SENSOR_WAIT;
    msg1.arg0 = train->nextSensor->num;
    Send(*SensorCourier, &msg1, sizeof(msg1), NULL, 0);
    CalibrationSnapshot(train);
}


static void setTrainSpeed(Train_t *train, int speed) {
    char buf[2];
    if (speed != train->speed) {
        updateLocation(train);
        train->transition->valid = true;
        train->transition->start_speed = train->speed;
        train->transition->dest_speed = speed;
        train->transition->time_issued = Time();
        train->transition->stopping_distance = getStoppingDistance(train->id, train->speed, speed);
        train->transition->stopping_distance += train->edgeDistance;
        train->speed = speed;
        debug("Dist to Next Sensor: %u, Current Edge Distance: %u, Stopping Distance: %u",
              train->distToNextSensor, train->edgeDistance, train->transition->stopping_distance);
        updateLocation(train);
    }
    buf[0] = train->speed + train->aux;
    buf[1] = train->id;
    trnputs(buf, 2);
}


static void TrainTask() {
    Train_t train;
    char command[2];
    TrainMessage_t request;
    TransitionState_t state;
    int status, bytes, callee;
    unsigned int dispatcher, speed;
    int SensorCourier, LocationTimer, ReverseCourier, waitingSensor;

    /* block on receive waiting for parent to send message */
    dispatcher = MyParentTid();
    Receive(&callee, &request, sizeof(request));
    if (callee != dispatcher || request.type != TRM_INIT) {
        error("Train: Error: Received message of type %d from %d, expected %d from %d",
              request.type, callee, TRM_INIT, dispatcher);
        Exit();
    }

    /* initialize state */
    state.valid = false;
    state.start_speed = 0;
    state.dest_speed = 0;
    state.time_issued = 0;

    /* initialize the train structure */
    train.id = request.tr;
    train.speed = 0;
    train.aux = 0;
    train.lastSensor = (track_node*)request.arg0;
    train.nextSensor = NULL;
    train.edgeDistance = 0;
    train.microPerTick = 0;
    train.lastUpdateTick = 0;
    train.transition = &state;
    train.distToNextSensor = 0;

    /* initialize couriers */
    SensorCourier = -1;
    ReverseCourier = -1;
    waitingSensor = -1;

    Reply(callee, NULL, 0);

    /* create the timer for location */
    updateNextSensor(&train);
    LocationTimer = Create(2, TrainTimer);

    while (true) {
        if ((bytes = Receive(&callee, &request, sizeof(request))) < 0) {
            error("TrainTask: Error: Received %d from %d", bytes, callee);
            status = -1;
            Reply(callee, &status, sizeof(status));
            continue;
        }

        status = 0;
        if (callee == SensorCourier) {
            if (train.nextSensor) {
                sensorTrip(&train, train.nextSensor);
                WaitOnNextTarget(&train, &SensorCourier, &waitingSensor);
            } else {
                setTrainSpeed(&train, 0);
            }
            CalibrationSnapshot(&train);
            continue;
        }

        switch (request.type) {
            case TRM_SPEED:
                speed = request.arg0;
                setTrainSpeed(&train, speed);
                WaitOnNextTarget(&train, &SensorCourier, &waitingSensor);
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_AUX:
                train.aux = request.arg0;
                command[0] = train.speed + train.aux;
                command[1] = train.id;
                trnputs(command, 2);
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_DIR:
                if (callee == ReverseCourier) {
                    train.lastSensor = train.nextSensor;
                    updateNextSensor(&train);
                    train.edgeDistance = train.distToNextSensor - train.edgeDistance;
                    command[0] = TRAIN_AUX_REVERSE;
                    command[1] = train.id;
                    trnputs(command, 2);
                }
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_RV:
                ReverseCourier = Create(3, TrainReverseCourier);
                request.type = TRM_RV;
                request.tr = train.id;
                request.arg0 = train.speed;
                Send(ReverseCourier, &request, sizeof(request), &status, sizeof(status));
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_GET_LOCATION:
                updateLocation(&train);
                request.arg0 = (int)train.lastSensor;
                request.arg1 = train.edgeDistance;
                Reply(callee, &request, sizeof(request));
                break;
            case TRM_GET_SPEED:
                status = train.speed;
                Reply(callee, &status, sizeof(status));
                break;
            default:
                error("TrainTask: Error: Bad request type %d from %d", request.type, callee);
        }
    }
    Exit();
}
