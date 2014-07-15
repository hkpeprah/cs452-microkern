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

#define GOTO_SPEED 10
#define STOP_BUFFER_MM 15
#define RESV_BUF_BITS 4

static track_edge *getNextEdge(track_node *node);

typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
    unsigned int stopping_distance;
} TransitionState_t;

typedef struct __TrainResvBuf {
    track_node *arr[(1 << RESV_BUF_BITS)];           // (path) of nodes reserved by this train
    unsigned int head : RESV_BUF_BITS;
    unsigned int tail : RESV_BUF_BITS;
    // TODO: keep track of resv dist in here so we can just
    // start counting from end of this buf
} TrainResv_t;

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

    int stoppingDist : 16;          // how long the train will go after speed is set to 0

    track_node **path;              // path the train is currently executing
    unsigned int pathNodeRem : 6;   // how many nodes are left in the path array
    unsigned int distOffset : 16;   // mm after last node in path to stop
    int pathRemain : 16;            // mm left until end of path

    TrainResv_t resv;

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
    snapshot.dist = train->distSinceLastSensor;
    snapshot.landmark = train->lastSensor->name;
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

uint32_t size_Resv(TrainResv_t *resv) {
    return (resv->tail - resv->head);
}

track_node *pop_head_Resv(TrainResv_t *resv) {
    // buffer not empty
    ASSERT(size_Resv(resv) > 0, "Train resv array empty");
    return resv->arr[resv->head++];
}

track_node *peek_back_Resv(TrainResv_t *resv) {
    return (size_Resv(resv) > 0) ? resv->arr[resv->tail - 1] : NULL;
}

track_node *peek_head_Resv(TrainResv_t *resv) {
    return (size_Resv(resv) > 0) ? resv->arr[resv->head] : NULL;
}

track_node *peek_any_Resv(TrainResv_t *resv, int index) {
    if (index >= size_Resv(resv)) {
        error("Index %d out of bounds, head %d, tail %d", index, resv->head, resv->tail);
        return NULL;
    }

    return resv->arr[resv->head + index];
}

void push_back_Resv(TrainResv_t *resv, track_node *node) {
    track_node *tail = peek_back_Resv(resv);
    Log("push_back_Resv : resv->head %d, resv->tail %d    ", resv->head, resv->tail);
    if (tail && node) {
        Log("tail: %s, node: %s, tail next node: %s\n", tail->name, node->name, getNextEdge(tail)->dest->name);
    }

    // buffer full
    ASSERT((resv->tail + 1) != resv->head, "Train resv array out of space");
    // empty or tail's next edge is what we want
    ASSERT((tail == NULL || getNextEdge(tail)->dest == node), "End of resv and next resv not equal");
    resv->arr[resv->tail++] = node;
}

void freeHeadResv(Train_t *train) {
    track_node *toFree = pop_head_Resv(&(train->resv));
    ASSERT(toFree != NULL, "Called freeHeadResv but nothing to free");
    Log("freeing %s", toFree->name);
    track_node *freed = DispatchReleaseTrack(train->id, &toFree, 1);
    ASSERT(freed == toFree, "Failed to free head of resv node");
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
static void reservePath(Train_t *train) {
    track_node *lastReserved, *expectedLastReserved = NULL;

    if (train->speed == 0) {
        return;
    }

    // 1.2 stopping distance of train
    int numNewResv = 0;
    track_node *toResv[16] = {0};
    int resvDist = MAX(0, train->stoppingDist - train->currentEdge->dist);

    if (train->path) {
        // has path, reserve along it
        ASSERT(train->currentEdge->dest == train->path[0], "Train edge dest is not equal to start of path");
        lastReserved = DispatchReserveTrackDist(train->id, train->path, train->pathNodeRem, &resvDist);
    } else {
        // no path, find nodes
        int i = 0;
        int distRemaining = resvDist;
        track_node *node = train->currentEdge->dest;
        track_edge *edge = getNextEdge(node);

        while (distRemaining > 0) {
            debug("reserving node %s to %s with distRemaining %d", node->name, edge->dest->name, distRemaining);
            if (peek_any_Resv(&(train->resv), i++) != node) {
                toResv[numNewResv++] = node;
            } else {
                debug("%s already reserved, skipping", node->name);
            }
            distRemaining -= edge->dist;
            node = edge->dest;
            edge = getNextEdge(node);
        }

        // overshoot so we have extra stopping dist, except when we don't need it
        if (node != peek_back_Resv(&(train->resv))) {
            // this is kind of hacky but... the initial acceleration phase
            // doesnt handle updateLocation properly to reserve new nodes
            toResv[numNewResv++] = node;
        }
        expectedLastReserved = toResv[numNewResv - 1];

        debug("reserving %d new nodes, dist remaining: %d", numNewResv, distRemaining);

        ASSERT(numNewResv < 16, "reservePath: overshot number of nodes to reserve");

        lastReserved = DispatchReserveTrack(train->id, toResv, numNewResv);
    }

    debug("Reserved up to %s", lastReserved->name);

    if (lastReserved != expectedLastReserved) {
        // reservation conflict! currently it just hard stops
        debug("failed to reserve up to %s (only to %s), stopping", expectedLastReserved->name, lastReserved->name);
        setTrainSpeed(train, 0);
    } else {
        Log("End of resv: %s\n", peek_back_Resv(&(train->resv))->name);
        // successful! add to our resvs
        int i;
        for (i = 0; i < numNewResv; ++i) {
            Log("resv path pushing back %d : %s\n", i, toResv[i]->name);
            push_back_Resv(&(train->resv), toResv[i]);
        }
    }

    /*
    if (train->path) {
        ASSERT(train->currentEdge->dest == train->path[0], "Train edge dest is not equal to start of path");
        // reserve path ahead, approximate via 1.5* stopping dist
        track_node *lastReserved = DispatchReserveTrackDist(train->id, train->path, train->pathNodeRem, train->stoppingDist * 15 / 10);

        int i = 0;
        int dist = 0;
        track_node **path = train->path;

        while (i < train->pathNodeRem - 1 && train->path[i] != lastReserved) {
            if (path[i]->edge[DIR_STRAIGHT].dest == path[i + 1]) {
                trainSwitch(path[i]->num, 'S');
                dist += path[i]->edge[DIR_STRAIGHT].dist;
            } else if (path[i]->type == NODE_BRANCH && path[i]->edge[DIR_CURVED].dest == path[i + 1]) {
                trainSwitch(path[i]->num, 'C');
                dist += path[i]->edge[DIR_CURVED].dist;
            } else {
                error("Error with path at node %s to %s", path[i]->name, path[i + 1]->name);
            }
            ++i;
        }

        if (dist <= train->stoppingDist) {
            // reservation in use! must stop now
            debug("Reservation failed after %s, stopping", lastReserved->name);
            setTrainSpeed(train, 0);
        }
    }
    */


    // YO MAX
    // WRITE A VERSION OF THE TRACK RESERVATION CALL THAT DOESNT CARE ABOUT THE PATH AND JUST USES THE CURRENT
    // SWITCH STATES TO RESERVE
    // PEACE -MAX

}


static void sensorTrip(Train_t *train, track_node *sensor) {
    int tick;

    ASSERT((train->nextSensor != NULL && sensor != NULL), "SensorTrip: NULL sensor");
    ASSERT(train->nextSensor == sensor, "SensorTrip: Expected sensor vs actual differ");
    ASSERT(sensor->type == NODE_SENSOR, "SensorTrip called with non-sensor");

    tick = Time();
    if (train->lastSensorTick >= 0 && !train->transition.valid) {
        train->microPerTick = (train->microPerTick >> 1) + (train->distToNextSensor * 500 / (tick - train->lastSensorTick));
    }

    train->transition.stopping_distance = MAX(0, train->transition.stopping_distance - train->distToNextSensor);

    train->lastUpdateTick = tick;
    train->lastSensorTick = tick;
    train->distSinceLastSensor = 0;
    train->distSinceLastNode = 0;
    train->lastSensor = sensor;
    train->currentEdge = getNextEdge(sensor);

    if (train->speed != 0) {
        track_node *resvHead;

        while (true)  {
            resvHead = peek_head_Resv(&(train->resv));

            if (!resvHead || resvHead == sensor) {
                break;
            }

            freeHeadResv(train);
        }

        ASSERT(resvHead != NULL, "Expected sensor to be resv but it was not");
        freeHeadResv(train);
        reservePath(train);
    }

    updateNextSensor(train);

    if (train->path) {
        ASSERT(train->path[0] == sensor, "tripped sensor but path[0] was not sensor");

        while (*(train->path) != sensor && train->pathNodeRem > 0) {
            train->path++;
            train->pathNodeRem--;
        }

        if (train->pathNodeRem > 0) {
            train->path++;
            train->pathNodeRem--;
        }

        train->pathRemain = pathRemaining(train);
        debug("compute path: %d", train->pathRemain);
    }
}


static void updateLocation(Train_t *train) {
    int ticks = Time();
    unsigned int traveledDist = 0;

    if (train->transition.valid) {
        int start_speed = train->transition.start_speed;
        int stop_speed = train->transition.dest_speed;
        if (ticks - train->transition.time_issued >= getTransitionTicks(train->id, start_speed, stop_speed)) {
            // stopped (or done accelerating?)
            train->transition.valid = false;
            train->distSinceLastSensor = train->transition.stopping_distance;
            train->microPerTick = getTrainVelocity(train->id, stop_speed);
            train->distSinceLastSensor += (ticks - train->transition.time_issued - getTransitionTicks(train->id, start_speed, stop_speed)) * train->microPerTick;

            train->distSinceLastNode = train->distSinceLastSensor;

            if (train->speed == 0) {
                // reserve where we stopped on
                track_node *sensor = DispatchReserveTrack(train->id, &(train->currentEdge->src), 1);
                ASSERT(sensor == train->lastSensor, "Train initial sensor reservation failed");
            }
        }
    } else {
        traveledDist += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    train->distSinceLastSensor += traveledDist;
    train->distSinceLastNode += traveledDist;
    train->pathRemain -= traveledDist;

    if (train->path && train->pathRemain < train->stoppingDist + STOP_BUFFER_MM) {
        train->path = NULL;
        setTrainSpeed(train, 0);
        debug("critical path remaining reached at %d for stopping dist %d, stopping", train->pathRemain, train->stoppingDist);
    }

    while ( train->distSinceLastNode > train->currentEdge->dist ) {
        if (train->currentEdge->dest == train->nextSensor) {
            // TODO: this is kind of concerning...
            //error("model beat train to %s", train->nextSensor->name);
            break;
        }

        ASSERT(train->currentEdge->dest == peek_head_Resv(&(train->resv)), "Traverse != resv");
        freeHeadResv(train);

        train->currentEdge = getNextEdge(train->currentEdge->dest);
        train->distSinceLastNode -= train->currentEdge->dist;

        if (train->path) {
            train->path++;
            train->pathNodeRem--;
        }

        reservePath(train);
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
        train->stoppingDist = getStoppingDistance(train->id, train->speed, speed);
        train->transition.valid = true;
        train->transition.start_speed = train->speed;
        train->transition.dest_speed = speed;
        train->transition.time_issued = tick;
        train->lastUpdateTick = tick;
        train->lastSensorTick = -1;
        debug("Stopping Distance for %d -> %d: %d", train->speed, speed, train->stoppingDist);
        if (speed == 0) {
            train->transition.stopping_distance = train->distSinceLastSensor + train->stoppingDist;
        } else {
            track_node *freed = DispatchReleaseTrack(train->id, &(train->currentEdge->src), 1);
            ASSERT(freed != NULL, "Failed to free last node (current edge src)");
            train->transition.stopping_distance = 0;
        }
        train->speed = speed;
        debug("Dist to Next Sensor: %u, Current Edge Distance: %u, Stopping Distance: %u",
              train->distToNextSensor, train->distSinceLastSensor, train->transition.stopping_distance);
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

    // reservation array
    int i;
    for (i = 0; i < (1 << RESV_BUF_BITS); ++i) {
        train->resv.arr[i] = 0;
    }
    train->resv.head = 0;
    train->resv.tail = 0;

    /* initialize transition */
    train->transition.valid = false;
    train->transition.start_speed = 0;
    train->transition.dest_speed = 0;
    train->transition.time_issued = 0;
    train->transition.stopping_distance = 0;

    // if train moved far enough past sensor, update it here
    // similar to updateLocation's update but this lets us get our initial reservation
    // whereas that guy also tries to free
    while (train->distSinceLastNode > train->currentEdge->dist) {
        train->currentEdge = getNextEdge(train->currentEdge->dest);
        train->distSinceLastNode -= train->currentEdge->dist;
    }

    updateNextSensor(train);

    // reserve your initial sensor
    track_node *sensor = DispatchReserveTrack(train->id, &(train->lastSensor), 1);
    ASSERT(sensor == train->lastSensor, "Train initial sensor reservation failed");
    push_back_Resv(&(train->resv), sensor);
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

    Reply(callee, NULL, 0);

    initTrain(&train, &request);

    /* initialize couriers */
    sensorCourier = -1;
    locationTimer = -1;
    reverseCourier = -1;
    waitingSensor = -1;


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
                    reservePath(&train);
                    waitOnNextTarget(&train, &sensorCourier, &waitingSensor);
                }

                break;
            case TRM_SPEED:
                speed = request.arg0;
                status = 1;
                Reply(callee, &status, sizeof(status));
                setTrainSpeed(&train, speed);
                reservePath(&train);
                waitOnNextTarget(&train, &sensorCourier, &waitingSensor);
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
