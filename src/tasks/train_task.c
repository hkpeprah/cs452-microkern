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

#define TIMEOUT_BUFFER 20

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
    track_edge *currentEdge;
    track_node *nextSensor;
    unsigned int edgeDistance : 16;
    unsigned int lastUpdateTick;
    unsigned int microPerTick : 16;
    unsigned int distToNextSensor;
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
    TRM_GET_PATH,
    TRM_DIR
} TrainMessageType;


typedef struct TrainMessage {
    TrainMessageType type;
    int arg0;
    int arg1;
    int arg2;
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
    TrainMessage_t request, message = {.type = TRM_DIR};
    int status, callee;

    Receive(&callee, &request, sizeof(request));
    status = 0;
    if (callee == MyParentTid()) {
        debug("ReverseCourier: Stopping train tid %u with starting speed %u", callee, request.arg0);
        Reply(callee, &status, sizeof(status));
        TrSpeed(callee, 0);
        Delay(getTransitionTicks(request.arg1, request.arg0, 0));
        debug("ReverseCourier: Reversing train tid %u", callee);
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        Delay(20);
        debug("ReverseCourier: Sending to speed up train tid %u to %u", callee, request.arg0);
        TrSpeed(callee, request.arg0);
    } else {
        status = -1;
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}


static track_edge *getNextEdge(track_node *node) {
    Switch_t *swtch;
    switch (node->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
            return &(node->edge[DIR_AHEAD]);
        case NODE_BRANCH:
            swtch = getSwitch(node->num);
            return &(node->edge[swtch->state]);
        case NODE_ENTER:
        case NODE_EXIT:
            return NULL;
        case NODE_NONE:
        default:
            error("getNextEdge: Error: Bad node type %u", node->type);
            return NULL;
    }
}


static void traverseNode(Train_t *train, track_node *dest) {
    train->lastUpdateTick = Time();
    if (dest->type == NODE_SENSOR) {
        if (train->transition->valid) {
            if (train->transition->stopping_distance >= train->distToNextSensor - train->edgeDistance) {
                train->transition->stopping_distance -= (train->distToNextSensor - train->edgeDistance);
            }
        }
        train->distToNextSensor = 0;
        train->edgeDistance = 0;
    }
    train->currentEdge = getNextEdge(dest);
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


static void WaitOnNextTarget(Train_t *train, int *SensorCourier, int *waitingSensor) {
    TrainMessage_t msg1;
    track_node *dest;
    track_edge *nextEdge;

    if (*waitingSensor >= 0) {
        FreeSensor(*waitingSensor);
    }

    if (*SensorCourier >= 0) {
        Destroy(*SensorCourier);
        *SensorCourier = -1;
    }

    dest = train->currentEdge->dest;
    train->distToNextSensor += train->currentEdge->dist;
    while (dest && dest->type != NODE_SENSOR) {
        nextEdge = getNextEdge(dest);
        dest = nextEdge->dest;
        train->distToNextSensor += nextEdge->dist;
    }

    train->nextSensor = dest;
    *waitingSensor = dest->num;
    *SensorCourier = Create(4, SensorCourierTask);
    msg1.type = TRM_SENSOR_WAIT;
    msg1.arg0 = dest->num;
    Send(*SensorCourier, &msg1, sizeof(msg1), NULL, 0);
    CalibrationSnapshot(train);
}


static void trainSpeed(Train_t *train, int speed) {
    char command[2];
    if (speed != train->speed) {
        /* compensate for acceleration */
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
    command[0] = train->speed + train->aux;
    command[1] = train->id;
    trnputs(command, 2);
}


static void TrainTimer() {
    TrainMessage_t msg = {.type = TRM_GET_LOCATION};
    int parent = MyParentTid();

    while (true) {
        Delay(10);
        Send(parent, &msg, sizeof(msg), NULL, 0);
    }

    Exit();
}


static void TrainTask() {
    short speed;
    TrainMessage_t request, message;
    Train_t train = {0};
    track_node *dest = NULL;
    char command[2], name[] = "TrainXX";
    int status, bytes, callee, waitingSensor;
    TransitionState_t state;
    int SensorCourier, ReverseCourier, timer;

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
    train.nextSensor = NULL;
    train.lastUpdateTick = 0;
    train.microPerTick = 0;
    train.edgeDistance = 0;
    train.transition = &state;
    train.distToNextSensor = 0;

    state.valid = false;

    name[5] = (train.id / 10) + '0';
    name[6] = (train.id % 10) + '0';
    ReverseCourier = 0;
    waitingSensor = -1;
    SensorCourier = -1;
    timer = Create(2, TrainTimer);

    message.type = TRM_TIME_WAIT;
    message.arg0 = (int)&train;

    timer = Create(2, TrainTimer);

    RegisterAs(name);
    Reply(callee, NULL, 0);

    while (true) {
        if ((bytes = Receive(&callee, &request, sizeof(request))) < 0) {
            error("TrainTask: Error: Received %d from %d", bytes, callee);
            Reply(callee, &status, sizeof(status));
            continue;
        }

        status = 0;
        if (callee == SensorCourier) {
            if (train.nextSensor) {
                traverseNode(&train, train.nextSensor);
            }

            CalibrationSnapshot(&train);
            WaitOnNextTarget(&train, &SensorCourier, &waitingSensor);
            if (train.nextSensor == dest) {
                trainSpeed(&train, 0);
                dest = NULL;
            }
            continue;
        }

        switch (request.type) {
            case TRM_TIME_READY:
                break;
            case TRM_SPEED:
                speed = request.arg0;
                trainSpeed(&train, speed);
                WaitOnNextTarget(&train, &SensorCourier, &waitingSensor);
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
                updateLocation(&train);
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            default:
                error("TrainTask: Error: Bad request type %d from %d", request.type, callee);
        }
    }

    Exit();
}
