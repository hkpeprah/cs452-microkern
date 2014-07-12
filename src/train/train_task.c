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

#define GOTO_SPEED 10

typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
    unsigned int stopping_distance;
} TransitionState_t;

typedef struct __Train_t {
    unsigned int id : 16;           // controller train id
    unsigned int speed : 8;         // controller train speed
    unsigned int aux : 16;          // functions such as light, horn, etc.
    unsigned int microPerTick : 16; // actual speed (micrometers / clock tick)

    track_node *lastSensor;
    track_node *nextSensor;
    track_edge *currentEdge;
    unsigned int distSinceLastSensor : 16;
    unsigned int distSinceLastNode : 16;
    int distToNextSensor : 16;      // distance between last sensor and next sensor

    unsigned int lastUpdateTick;    // last time the train position was updated
    int lastSensorTick;             // last time the train triggered a sensor

    int stoppingDist;               // how long the train will go after speed is set to 0

    track_node **path;              // path the train is currently executing
    unsigned int pathNodeRem : 6;   // how many nodes are left in the path array
    unsigned int distOffset : 16;   // mm after last node in path to stop
    unsigned int pathRemain : 16;   // mm left until end of path

    TransitionState_t transition;
} Train_t;

typedef enum {
    TRM_INIT = 2873,
    TRM_SENSOR_WAIT,
    TRM_GOTO_AFTER,
    TRM_SPEED,
    TRM_AUX,
    TRM_DIR,
    TRM_RV,
    TRM_GET_LOCATION,
    TRM_GET_NEXT_LOCATION,
    TRM_GET_SPEED
} TrainMessageType;

typedef struct TrainMessage {
    TrainMessageType type;
    unsigned int tr;
    int arg0;
    int arg1;                                                                                         
    int arg2;
} TrainMessage_t;

static void TrainTask();
static void setTrainSpeed(Train_t *train, int speed);

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

    TrAuxiliary(tr, 16);

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


int TrAuxiliary(unsigned int tid, unsigned int aux) {
    TrainMessage_t msg = {.type = TRM_AUX, .arg0 = aux};
    int status, bytes;

    if ((bytes = Send(tid, &msg, sizeof(msg), &status, sizeof(status))) < 0) {
        return bytes;
    }
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

int TrGotoAfter(unsigned int tid, track_node **path, unsigned int pathLen,  unsigned int dist) {
    TrainMessage_t msg = {.type = TRM_GOTO_AFTER, .arg0 = (int) path, .arg1 = pathLen, .arg2 = dist};
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

track_node *TrGetNextLocation(unsigned int tid, unsigned int *distance) {
    TrainMessage_t msg = {.type = TRM_GET_NEXT_LOCATION};

    Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    *distance = msg.arg1;
    return (track_node*)msg.arg0;
}

static void CalibrationSnapshot(Train_t *train) {
    CalibrationSnapshot_t snapshot;
    snapshot.tr = train->id;
    snapshot.sp = train->microPerTick;
    snapshot.dist = train->distSinceLastNode;
    snapshot.landmark = train->currentEdge->src->name;
    snapshot.nextmark = train->nextSensor->name;
    if (train->lastSensorTick < 0) {
        snapshot.eta = 0;
        snapshot.ata = 0;
    } else {
        snapshot.eta = (train->distToNextSensor * 1000) / train->microPerTick + train->lastSensorTick;
        snapshot.ata = train->lastSensorTick < 0 ? 0 : train->lastSensorTick;
    }
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

/***** ACTUAL, HARD-WORKING AMERICAN FUNCTOINS START HERE *****/

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

// compute distance till end of path
static uint32_t pathRemaining(const Train_t *train) {
    track_node **path = train->path;
    uint32_t pathDist = train->currentEdge->dist - train->distSinceLastNode + train->distOffset;
    int nextNodeDist;

    int i;
    for (i = 0; i < train->pathNodeRem - 1; ++i) {
        if ( (nextNodeDist = validNextNode(path[i], path[i+1])) == INVALID_NEXT_NODE ) {
            Panic("Invalid nodes in train (%d) path: %s to %s", train->id, path[i]->name, path[i+1]->name);
            return nextNodeDist;
        }
        pathDist += nextNodeDist;
    }

    return pathDist;
}

// request path
// flip switches
// compute path distance, stop if necessary
static void trainPath(Train_t *train) {
}


static void sensorTrip(Train_t *train, track_node *sensor) {
    int tick;

    if (sensor->type != NODE_SENSOR) {
        error("SensorTrip: Error: Called with non-sensor node %s", sensor->name);
    }

    tick = Time();
    if (train->lastSensorTick >= 0 && !train->transition.valid) {
        train->microPerTick = (train->microPerTick >> 1) + (train->distToNextSensor * 500 / (tick - train->lastSensorTick));
    }

    if (train->transition.stopping_distance >= train->distToNextSensor - 10) {
        train->transition.stopping_distance = MAX(0, train->transition.stopping_distance - train->distToNextSensor);
    }

    train->lastUpdateTick = tick;
    train->lastSensorTick = tick;
    train->distSinceLastSensor = 0;
    train->distSinceLastNode = 0;
    train->lastSensor = sensor;
    train->currentEdge = getNextEdge(sensor);

    if (train->path) {
        // assert here
        if (train->path[0] != sensor) {
            error("expected path to be at %s but it is at %s", sensor->name, train->path[0]->name);
        }

        while (*(train->path) != sensor) {
            train->path++;
            train->pathNodeRem--;
        }

        train->path++;
        train->pathNodeRem--;

        train->pathRemain = pathRemaining(train);
        debug("pathRemain: %d", train->pathRemain);
    }

    updateNextSensor(train);
}


static void updateLocation(Train_t *train) {
    int ticks;
    unsigned int start_speed, stop_speed;
    unsigned int traveledDist = 0;

    ticks = Time();
    if (train->transition.valid) {
        start_speed = train->transition.start_speed;
        stop_speed = train->transition.dest_speed;
        if (ticks - train->transition.time_issued >= getTransitionTicks(train->id, start_speed, stop_speed)) {
            train->transition.valid = false;
            train->distSinceLastSensor = train->transition.stopping_distance;
            train->microPerTick = getTrainVelocity(train->id, stop_speed);
            train->distSinceLastSensor += (ticks - train->transition.time_issued - getTransitionTicks(train->id, start_speed, stop_speed)) * train->microPerTick;

            train->distSinceLastNode = train->distSinceLastSensor;
        }
    } else {
        traveledDist += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    train->distSinceLastSensor += traveledDist;
    train->distSinceLastNode += traveledDist;
    train->pathRemain -= traveledDist;

    if (train->path && train->pathRemain <= train->stoppingDist - 20) {
        train->path = NULL;
        setTrainSpeed(train, 0);
    }

    int numTraverse = 0;

    while ( train->distSinceLastNode > train->currentEdge->dist ) {
        if (train->currentEdge->dest == train->nextSensor) {
            break;
        }
        debug("traversing to %s and freeing %s", train->currentEdge->dest->name, train->currentEdge->src->name);
        numTraverse++;
        train->currentEdge = getNextEdge(train->currentEdge->dest);
        train->distSinceLastNode -= train->currentEdge->dist;
    }

    if (numTraverse > 0) {
        // TODO: free traversed
        // TODO: reserve, recompute path dist, stop if necessary
        train->path += numTraverse;
        train->pathNodeRem -= numTraverse;
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
        error("SensorCourier: Expected callee to be %d but was %d", train, callee);
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
        error("SensorCourier: Error: WaitOnSensorN returned %d", status);
    }
    // debug("SensorCourier: Tripped sensor %d", wait);
    Send(train, NULL, 0, NULL, 0);
}

static void waitOnNextTarget(Train_t *train, int *sensorCourier, int *waitingSensor) {
    TrainMessage_t msg1;

    if (*waitingSensor >= 0) {
        FreeSensor(*waitingSensor);
    }

    if (*sensorCourier >= 0) {
        Destroy(*sensorCourier);
        *sensorCourier = -1;
    }

    // potentially need to call updateNextSensor here? only if track state changed between
    // last sensor trip and this

    if (train->nextSensor) {
        *waitingSensor = train->nextSensor->num;
        *sensorCourier = Create(4, SensorCourierTask);
        msg1.type = TRM_SENSOR_WAIT;
        msg1.arg0 = train->nextSensor->num;
        Send(*sensorCourier, &msg1, sizeof(msg1), NULL, 0);
    }
    CalibrationSnapshot(train);
}


static void setTrainSpeed(Train_t *train, int speed) {
    char buf[2];
    if (speed != train->speed) {
        int tick = Time();
        int stoppingDistance = getStoppingDistance(train->id, train->speed, speed);
        train->transition.valid = true;
        train->transition.start_speed = train->speed;
        train->transition.dest_speed = speed;
        train->transition.time_issued = tick;
        train->lastUpdateTick = tick;
        train->lastSensorTick = -1;
        train->stoppingDist = stoppingDistance;
        debug("Stopping Distance for %d -> %d: %d", train->speed, speed, train->stoppingDist);
        if (speed == 0) {
            train->transition.stopping_distance = stoppingDistance;
            train->distSinceLastNode += stoppingDistance;
        } else {
            train->transition.stopping_distance = 0;
        }
        train->speed = speed;
        debug("Dist to Next Sensor: %u, Current Edge Distance: %u, Stopping Distance: %u",
              train->distToNextSensor, train->distSinceLastSensor, train->transition.stopping_distance);
        updateLocation(train);
    }
    buf[0] = train->speed + train->aux;
    buf[1] = train->id;
    trnputs(buf, 2);
}

static void initTrain(Train_t *train, TrainMessage_t *request) {
    /* initialize the train structure */
    train->id = request->tr;
    train->speed = 0;
    train->aux = 0;
    train->microPerTick = 0;

    train->lastSensor = (track_node*)request->arg0;
    train->nextSensor = NULL;
    train->currentEdge = getNextEdge(train->lastSensor);
    train->distSinceLastSensor = getStoppingDistance(train->id, 3, 0);
    train->distSinceLastNode = train->distSinceLastSensor;
    train->distToNextSensor = 0;

    train->lastUpdateTick = 0;
    train->lastSensorTick = -1;

    train->stoppingDist = -2;

    train->path = NULL;
    train->pathNodeRem = 0;
    train->distOffset = 0;
    train->pathRemain = 0;


    /* initialize transition */
    train->transition.valid = false;
    train->transition.start_speed = 0;
    train->transition.dest_speed = 0;
    train->transition.time_issued = 0;
    train->transition.stopping_distance = 0;
}


static void TrainTask() {
    Train_t train;
    char command[2];
    TrainMessage_t request;
    int status, bytes, callee;
    unsigned int dispatcher, speed;
    int sensorCourier, locationTimer, reverseCourier, waitingSensor;
    int gotoBlocked = 0;

    /* block on receive waiting for parent to send message */
    dispatcher = MyParentTid();
    Receive(&callee, &request, sizeof(request));
    if (callee != dispatcher || request.type != TRM_INIT) {
        error("Train: Error: Received message of type %d from %d, expected %d from %d",
              request.type, callee, TRM_INIT, dispatcher);
        Exit();
    }

    initTrain(&train, &request);

    /* initialize couriers */
    sensorCourier = -1;
    locationTimer = -1;
    reverseCourier = -1;
    waitingSensor = -1;

    Reply(callee, NULL, 0);

    updateNextSensor(&train);
    locationTimer = Create(2, TrainTimer);

    while (true) {
        if ((bytes = Receive(&callee, &request, sizeof(request))) < 0) {
            error("TrainTask: Error: Received %d from %d", bytes, callee);
            status = -1;
            Reply(callee, &status, sizeof(status));
            continue;
        }

        status = 0;
        if (callee == sensorCourier) {
            if (train.nextSensor) {
                sensorTrip(&train, train.nextSensor);
                waitOnNextTarget(&train, &sensorCourier, &waitingSensor);
            } else {
                setTrainSpeed(&train, 0);
            }
            CalibrationSnapshot(&train);
            continue;
        }

        switch (request.type) {
            case TRM_GOTO_AFTER:
                // blocking call, reply when we arrive
                gotoBlocked = callee;
                train.path = (track_node**) request.arg0;
                train.pathNodeRem = request.arg1;
                train.distOffset = request.arg2;
                train.pathRemain = pathRemaining(&train);

                if (train.pathRemain < 5) {
                    // short move to it
                    debug("short move!");
                } else {
                    debug("normal move!");
                    setTrainSpeed(&train, 10);
                    waitOnNextTarget(&train, &sensorCourier, &waitingSensor);
                }

                break;
            case TRM_SPEED:
                speed = request.arg0;
                setTrainSpeed(&train, speed);
                waitOnNextTarget(&train, &sensorCourier, &waitingSensor);
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
                if (train.speed != 0 || train.transition.valid) {
                    error("Cannot reverse while moving");
                    status = -1;
                    Reply(callee, &status, sizeof(status));
                    break;
                }
                train.distSinceLastNode = train.currentEdge->dist - train.distSinceLastNode;
                train.currentEdge = train.currentEdge->reverse;
                status = 0;
                command[0] = TRAIN_AUX_REVERSE;
                command[1] = train.id;
                trnputs(command, 2);
                Reply(callee, &status, sizeof(status));

                break;
            case TRM_RV:
                // TODO: replace me with github version 
                break;
            case TRM_GET_LOCATION:
                updateLocation(&train);
                request.arg0 = (int)train.currentEdge->src;
                request.arg1 = train.distSinceLastNode;
                Reply(callee, &request, sizeof(request));

                if (train.speed == 0 && !train.transition.valid) {
                    Reply(gotoBlocked, NULL, 0);
                }
                break;
            case TRM_GET_NEXT_LOCATION:
                request.arg0 = (int)train.currentEdge->dest;
                request.arg1 = train.currentEdge->dist - train.distSinceLastNode;
                Reply(callee, &request, sizeof(request));
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
