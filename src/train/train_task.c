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
#include <logger.h>

#define GOTO_SPEED              10
#define STOP_BUFFER_MM          15
#define RESV_BUF_BITS           4
#define PICKUP_FRONT            0
#define PICKUP_BACK             0
#define DEAD_SENSOR_BUFFER      150  // 150 mm before we skip over a sensor without tripping it
#define DELAY_PRIORITY          8
#define RESV_MOD                ((1 << RESV_BUF_BITS) - 1)
#define RESV_BUFFER             200
#define RESV_DIST(req_dist)     ((req_dist * 5) >> 2)
#define SENSOR_MISS_THRESHOLD   2

typedef enum {
    STOP = 9483,
    CONST_SP,
    ACCEL,
    DECEL,
    SHORT_MV
} TrainState_t;


typedef struct {
    TrainState_t state;
    unsigned int num_ticks : 16;
    unsigned int end_velo : 16;
    unsigned int time_issued;
    unsigned int stopping_distance;
} TransitionState_t;

typedef struct __TrainResvBuf {
    track_node *arr[(1 << RESV_BUF_BITS)];           // (path) of nodes reserved by this train
    unsigned int head;
    unsigned int tail;
    int extraResvDist;                               // extra distance reserved past our stopping dist
    // TODO: keep track of resv dist in here so we can just
    // start counting from end of this buf
} TrainResv_t;

typedef struct __Train_t {
    unsigned int id : 16;                  // controller train id
    unsigned int speed : 8;                // controller train speed
    unsigned int aux : 16;                 // functions such as light, horn, etc.
    unsigned int microPerTick : 16;        // actual speed (micrometers / clock tick)
    track_node *lastSensor;
    track_node *nextSensor;
    track_edge *currentEdge;
    int distSinceLastSensor : 16;
    int distSinceLastNode : 16;
    int distToNextSensor : 16;             // distance between last sensor and next sensor

    unsigned int lastUpdateTick;           // last time the train position was updated
    int lastSensorTick;                    // last time the train triggered a sensor

    int stoppingDist : 16;                 // how long the train will go after speed is set to 0

    track_node **path;                     // path the train is currently executing
    unsigned int distOffset : 16;          // mm after last node in path to stop
    int pathRemain : 16;                   // mm left until end of path
    unsigned int pathNodeRem : 6;          // how many nodes are left in the path array
    GotoResult_t gotoResult;

    unsigned int pickupOffset : 8;

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
    TRM_GET_EDGE,
    TRM_GET_SPEED,
    TRM_DELETE,
    TRM_ROUTE_COMPLETE
} TrainMessageType;

typedef struct TrainMessage {
    TrainMessageType type;
    unsigned int tr;
    int arg0;
    int arg1;                                                                                         
    int arg2;
} TrainMessage_t;

static void TrainTask();
static void TrainReverseCourier();
static void setTrainSpeed(Train_t *train, int speed);
static track_node *peek_any_resv(TrainResv_t *resv, uint32_t index);
static track_node *peek_back_resv(TrainResv_t *resv);
static track_node *peek_head_resv(TrainResv_t *resv);


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


bool TrRouteComplete(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_ROUTE_COMPLETE};
    int status, errno;

    errno = Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("TrRouteComplete: Got %d sending to %d from %d", errno, tid, MyTid());
        return false;
    }

    if (status) {
        return true;
    }
    return false;
}


int TrDelete(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_DELETE};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrSpeed(unsigned int tid, unsigned int speed) {
    TrainMessage_t msg = {.type = TRM_SPEED, .arg0 = speed};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrDir(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_DIR};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrReverse(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_RV};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrDirection(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_DIR};
    int status;

    Send(tid, &msg, sizeof(msg), &status, sizeof(status));
    return status;
}


int TrAuxiliary(unsigned int tid, unsigned int aux) {
    TrainMessage_t msg = {.type = TRM_AUX, .arg0 = aux};
    int status, bytes;

    if (aux != TRAIN_LIGHT_OFFSET && aux != TRAIN_HORN_OFFSET && aux < 32) {
        return INVALID_AUXILIARY;
    }

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


GotoResult_t TrGotoAfter(unsigned int tid, track_node **path, unsigned int pathLen,  unsigned int dist) {
    TrainMessage_t msg = {.type = TRM_GOTO_AFTER, .arg0 = (int)path, .arg1 = pathLen, .arg2 = dist};
    int bytes;
    GotoResult_t result;


    if ((bytes = Send(tid, &msg, sizeof(msg), &result, sizeof(result))) < 0) {
        return bytes;
    }
    return result;
}


track_node *TrGetLocation(unsigned int tid, unsigned int *distance) {
    TrainMessage_t msg = {.type = TRM_GET_LOCATION};

    Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    *distance = msg.arg1;

    return (track_node*)msg.arg0;
}


track_edge *TrGetEdge(unsigned int tid) {
    TrainMessage_t msg = {.type = TRM_GET_EDGE};
    int status;

    status = Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    if (status < 0) {
        error("TrGetEdge: Got %d in send to train with tid %d", status, tid);
    }
    return (track_edge*)msg.arg0;
}

#define PTL(train) printTrainLocation(train, __FUNCTION__, __LINE__)

static void printTrainLocation(const Train_t *train, const char *function, const int line) {
    debug("<%s:%d> Train %d at %d/%d after %s",
            function, line, train->id, train->distSinceLastNode, d(train->currentEdge).dist, train->currentEdge->src->name);
}

static void CalibrationSnapshot(Train_t *train) {
    CalibrationSnapshot_t snapshot;
    snapshot.tr = train->id;
    snapshot.sp = train->microPerTick;
    snapshot.dist = train->distSinceLastNode;
    snapshot.landmark = d(train->currentEdge).src->name;
    snapshot.nextmark = (train->nextSensor != NULL ? d(train->nextSensor).name : "NONE");
    if (train->lastSensorTick < 0) {
        snapshot.eta = 0;
        snapshot.ata = 0;
    } else {
        snapshot.eta = (train->distToNextSensor * 1000) / train->microPerTick + train->lastSensorTick;
        snapshot.ata = train->lastSensorTick < 0 ? 0 : train->lastSensorTick;
    }
    track_node *n;
    snapshot.headResv = ((n = peek_head_resv(&(train->resv))) ? n->name : "NULL");
    snapshot.tailResv = ((n = peek_back_resv(&(train->resv))) ? n->name : "NULL");
    printTrainSnapshot(&snapshot);
}


static void TrainTimer() {
    int parent;

    parent = MyParentTid();
    while (true) {
        Delay(5);
        if (Send(parent, NULL, 0, NULL, 0) < 0) {
            /* if we fail to send to the parent, kill ourselves */
            break;
        }
    }
    Exit();
}

/***** ACTUAL, HARD-WORKING AMERICAN FUNCTOINS START HERE *****/

static uint32_t size_resv(TrainResv_t *resv) {
    uint32_t size = (resv->tail - resv->head) & RESV_MOD;
    return size;
}


static void dump_resv(TrainResv_t *resv) {
    // should be debugging only
    int i;
    for (i = 0; i < size_resv(resv); ++i) {
        Log("%s", d(peek_any_resv(resv, i)).name);
    }
}

static track_node *pop_back_resv(TrainResv_t *resv) {
    if (size_resv(resv) <= 0) {
        return NULL;
    }
    resv->tail = (resv->tail - 1) & RESV_MOD;
    return resv->arr[resv->tail];
}

static track_node *pop_head_resv(TrainResv_t *resv) {
    if (size_resv(resv) <= 0) {
        return NULL;
    }
    int ind = resv->head;
    resv->head = (resv->head + 1) & RESV_MOD;
    return resv->arr[ind];
}


static track_node *peek_back_resv(TrainResv_t *resv) {
    int ind = (resv->tail - 1) & RESV_MOD;
    return (size_resv(resv) > 0) ? resv->arr[ind] : NULL;
}


static track_node *peek_head_resv(TrainResv_t *resv) {
    return (size_resv(resv) > 0) ? resv->arr[resv->head] : NULL;
}


static track_node *peek_any_resv(TrainResv_t *resv, uint32_t index) {
    if (index >= size_resv(resv)) {
        // error("Index %d out of bounds, head %u, tail %u", index, resv->head, resv->tail);
        return NULL;
    }

    return resv->arr[(resv->head + index) & RESV_MOD];
}


static void push_back_resv(TrainResv_t *resv, track_node *node) {
    track_node *tail = peek_back_resv(resv);

    if (tail == node || (tail && tail->reverse == node)) {
        /* addresses case where we reserve something that we already have */
        return;
    }
    // buffer full
    ASSERT((resv->tail + 1) != resv->head, "Train resv array out of space");
    // empty or tail's next edge is what we want
    ASSERT((tail == NULL || validNextNode(tail, node) != INVALID_NEXT_NODE) ||
           (validNextNode(tail->reverse, node) != INVALID_NEXT_NODE),
           "End of resv(%s) and next resv(%s) not equal", tail->name, node->name);
    int ind = resv->tail;
    resv->tail = (resv->tail + 1) & RESV_MOD;
    resv->arr[ind] = node;
}


static int find_resv(TrainResv_t *resv, track_node *node) {
    int i;
    for (i = 0; i < size_resv(resv); ++i) {
        if (peek_any_resv(resv, i) == node) {
            return i;
        }
    }
    return -1;
}


static int freeHeadResv(Train_t *train) {
    track_node *toFree = pop_head_resv(&(train->resv));
    if (toFree) {
        int freed = DispatchReleaseTrack(train->id, &toFree, 1);
        ASSERT(freed == 1, "Failed to free head of resv node %s (reserved by %u)",
               toFree->name, toFree->reservedBy);
        Log("Train %u: Freed head %s", train->id, toFree->name);
        return 0;
    }
    return -1;
}


static int freeTailResv(Train_t *train) {
    track_node *toFree = pop_back_resv(&(train->resv));
    if (toFree) {
        int freed = DispatchReleaseTrack(train->id, &toFree, 1);
        ASSERT(freed == 1, "Failed to free tail of resv node %s (reserved by %u)",
               toFree->name, toFree->reservedBy);
        Log("Train %u: Freed tail %s", train->id, toFree->name);
        return 0;
    }
    return -1;
}


static void updateNextSensor(Train_t *train) {
    int i, nextNodeDist;
    track_edge *edge;
    track_node *node, **path;

    i = 0;
    node = d(train->currentEdge).dest;
    edge = NULL;
    train->distToNextSensor = MAX(0, (int) train->currentEdge->dist - train->distSinceLastNode);
    path = train->path;

    if (train->currentEdge->dest->type == NODE_SENSOR) {
        // current dest is a sensor, no matter what that will be next sensor
        train->nextSensor = train->currentEdge->dest;
        return;
    }

    do {
        if (path != NULL && path[i] != NULL && i < train->pathNodeRem - 1) {
            nextNodeDist = validNextNode(path[i], path[i + 1]);
            ASSERT(nextNodeDist != INVALID_NEXT_NODE, "Train's path is wrong: %s([%d]) to %s([%d])",
                   d(path[i]).name, i, d(path[i + 1]).name, i + 1);
            train->distToNextSensor += nextNodeDist;
            node = path[++i];
        } else {
            edge = getNextEdge(node);
            if (edge == NULL) {
                if (train->transition.state == CONST_SP || train->transition.state == ACCEL) {
                    /* instead of reversing, just stop moving if we're reaching an end */
                    Log("Train %u: Reached exit node %s", train->id, d(node).name);
                    setTrainSpeed(train, 0);
                }
                node = NULL;
            } else {
                train->distToNextSensor += d(edge).dist;
                node = d(edge).dest;
            }
        }
    } while (node != NULL && node->type != NODE_SENSOR);

    if ((train->nextSensor = node)) {
        Log("Train %u -> %s with dist %d",
            train->id, node->name, train->distToNextSensor);
    }
}


static uint32_t pathRemaining(const Train_t *train) {
    if (!train->path || train->pathNodeRem <= 0) {
        return -1;
    }
    /* computes the distance to the end of a path */
    int i, nextNodeDist;

    track_node **path = train->path;
    uint32_t pathDist = train->currentEdge->dist - train->distSinceLastNode + train->distOffset + train->pickupOffset;

    Log("Train %u: path computation: edge total dist (%d), train at (%d) past src (%s)",
        train->id, train->currentEdge->dist, train->distSinceLastNode, train->currentEdge->src->name);

    for (i = 0; i < train->pathNodeRem - 1; ++i) {
        if ((nextNodeDist = validNextNode(path[i], path[i+1])) == INVALID_NEXT_NODE) {
            Panic("Invalid nodes in train (%d) path: %s to %s", train->id, path[i]->name, path[i+1]->name);
        }
        pathDist += nextNodeDist;
    }

    return pathDist;
}


static void freeResvOnStop(Train_t *train) {
    track_node *node;

    ASSERT(train->currentEdge, "Train on NULL edge?!");

    while ((node = peek_head_resv(&train->resv))) {
        if (node == train->currentEdge->src || node == train->currentEdge->dest) {
            break;
        }
        ASSERT(freeHeadResv(train) >= 0, "WAT");
    }

    while ((node = peek_back_resv(&train->resv))) {
        if (node == train->currentEdge->src || node == train->currentEdge->dest) {
            break;
        }
        ASSERT(freeTailResv(train) >= 0, "WAT");
    }

    Log("Train %u: After freeResvOnStop @ %s", train->id, train->currentEdge->src->name);
    dump_resv(&(train->resv));
}


// request path
// returns amount of dist left to reserve (can be negative, ie. there's extra)
static int reserveTrack(Train_t *train, int resvDist) {
    int numResv, numSuccessResv;
    track_node *resvArr[RESV_MOD] = {0};
    track_node **toResv = resvArr;


    if (train->path && train->pathNodeRem > 0) {
        /* train has a path, need to reserve along it */
        if (train->currentEdge->dest != train->path[0]
                && train->currentEdge->dest->reverse != train->path[0]
                && find_resv(&(train->resv), train->path[0]) == -1
                && train->transition.state == STOP) {
            // oh no! we're a bit lost! do a short path find to train->path[0]
            // if it still fails, give up and ask to be rerouted
            Log("Train %u: overshot path, edge dest %s but path[0] %s", train->id,
                d(train->currentEdge->dest).name, d(train->path[0]).name);

            track_node *overshotComp[4];
            int ospathLen;
            uint32_t totalDistance;

            if ((ospathLen = findPath(train->id, train->currentEdge, train->path[0], overshotComp, 4, &totalDistance)) < 0) {
                // fail, return positive number
                Log("Train %u: findPath failed on overshot correction, stopping", train->id);
                return 1;
            }

            int extraPathDist = totalDistance + train->currentEdge->dist - train->distSinceLastNode;

            // modify our look-ahead values (next sensor, end of path) by the overshoot distance
            train->pathRemain += extraPathDist;
            if (train->currentEdge->dest->type != NODE_SENSOR) {
                train->distToNextSensor += extraPathDist;
            }

            Log("Train %u: Resv overshot path of length %d, adding %d to path for a total of %d",
                train->id, totalDistance, extraPathDist, train->pathRemain);

            // otherwise, we found a way back to the head of path!
            int i, numSuccessResv = DispatchReserveTrack(train->id, overshotComp, ospathLen);

            dump_resv(&(train->resv));
            for (i = 0; i < numSuccessResv; ++i) {
                Log("%d : %s", i, d(overshotComp[i]).name);
                push_back_resv(&(train->resv), overshotComp[i]);
            }

            if (ospathLen != numSuccessResv) {
                // ... but failed to reserve it
                Log("Train %u: reserve failed on overshot correction, stopping", train->id);
                return 1;
            }
        }

        Log("Train %u: reservation on train path at %s for %d nodes", train->id, train->path[0]->name, train->pathNodeRem);
        toResv = train->path;
        numResv = train->pathNodeRem;
        Log("reserveTrack: Train %u: Path exists for train %u, toResv = %x, numResv = %d\n", train->id,
            train->id, toResv, numResv);
    } else {
        if (train->currentEdge->dest != NULL) {
            toResv = &(train->currentEdge->dest);
        } else {
            toResv = &(train->currentEdge->src);
        }
        numResv = 1;
    }
    Log("Train %u calling DispatchReserveTrackDist with toResv = %s, numResv = %d", train->id, toResv[0]->name, numResv);
    numSuccessResv = DispatchReserveTrackDist(train->id, toResv, numResv, &resvDist);

    track_edge *lastResvEdge = NULL;
    track_node *lastResvNode = NULL;

    // find the tail before reserving - this is so that if only 1 node is resvd
    // then we have to subtract the edge distance ourself
    if ((lastResvNode = peek_back_resv(&(train->resv)))) {
        lastResvEdge = getNextEdge(lastResvNode);
        if (numSuccessResv > 0 && lastResvEdge && lastResvEdge->dest == toResv[numResv - 1]) {
            resvDist -= lastResvEdge->dist;
        }
    }

    int i, osResvBase;
    track_node *nextResv = NULL;

    if ((osResvBase = find_resv(&(train->resv), toResv[0])) < 0) {
        osResvBase = 0;
    }

    int minReserve = MIN(numSuccessResv, numResv);

    for (i = 0; i < numSuccessResv; ++i) {
        if (i < minReserve) {
            // everything we told it to reserve
            nextResv = toResv[i];
        } else {
            // anything extra due to dist-based reserve
            ASSERT(nextResv, "nextResv is NULL");
            nextResv = d(getNextEdge(nextResv)).dest;
        }

        if (peek_any_resv(&(train->resv), osResvBase + i) == nextResv) {
            continue;
        }

        ASSERT(nextResv->reservedBy == train->id,
               "Whoa, pushing back node %s reserved by %d on train %d", nextResv->name, nextResv->reservedBy, train->id);
        push_back_resv(&(train->resv), nextResv);
    }

    Log("End of resv:");
    dump_resv(&(train->resv));

    if (train->path && peek_back_resv(&(train->resv))->type == NODE_EXIT && resvDist >= 0) {
        return -1;
    }

    return resvDist;
}


// 0 success, -1 fail
static int flipSwitch(track_node *sw, track_node *next) {
    ASSERT(sw && next, "flipSwitch: provided nodes contain NULL");
    ASSERT(sw->type == NODE_BRANCH, "flipSwitch: provided sw %s is not a switch", sw->name);

    if (sw->edge[DIR_STRAIGHT].dest == next) {
        ASSERT(trainSwitch(sw->num, 'S') == 0, "Switch on %s to S failed", sw->name);
    } else if (sw->edge[DIR_CURVED].dest == next) {
        ASSERT(trainSwitch(sw->num, 'C') == 0, "Switch on %s to C failed", sw->name);
        if (sw->num == 156 || sw->num == 154) {
            ASSERT(trainSwitch(sw->num - 1, 'S') == 0, "Switch on %d to S failed", sw->name);
        }
    } else {
        return -1;
    }
    return 0;
}

static void execPath(Train_t *train, track_node *lastExec) {
    track_node *current;
    track_node *next;
    int i;

    if (!train->path || train->pathNodeRem <= 0) {
        return;
    }

    Log("execPath for train %u", train->id);
    if (train->currentEdge->dest != train->path[0]) {
        Log("WARNING: execPath: Train edge dest(%s) is not equal to start of path(%s)",
            d(train->currentEdge->dest).name, d(train->path[0]).name);
    }

    if (peek_head_resv(&(train->resv)) != train->path[0]) {
        /* TODO: head of the train reservation is NULL, but path is not ? */
        Log("WARNING: execPath: Expected resv (%s) to be path[0] but got %s",
            d(peek_head_resv(&(train->resv))).name, d(train->path[0]).name);
    }

    int resvSize = size_resv(&(train->resv));

    i = 0;
    do {
        current = peek_any_resv(&(train->resv), i++);
    } while (lastExec && current != lastExec && i < resvSize);

    Log("starting path exec at resv index %d node %s for train %u", i, d(current).name, train->id);
    for (; i < resvSize; ++i) {
        next = peek_any_resv(&(train->resv), i);
        if (d(current).type == NODE_BRANCH) {
            if (flipSwitch(current, next) < 0) {
                error("Train %u asked to switch %s to reach %s, impossibru!", train->id, current->name, d(next).name);
            }
        }
        current = next;
    }

    Log("exec path finished");
}


static void sensorTrip(Train_t *train, track_node *sensor) {
    /* if train->nextSensor is NULL, we've reached an exit node */
    if (train->nextSensor == NULL) {
        return;
    }

    ASSERT((train->nextSensor != NULL && sensor != NULL), "SensorTrip: NULL sensor");
    ASSERT(train->nextSensor == sensor, "SensorTrip: Expected sensor(%s) vs actual differ(%s)",
           train->nextSensor->name, sensor->name);
    ASSERT(sensor->type == NODE_SENSOR, "SensorTrip: Called with non-sensor: Type %d", sensor->type);

    int tick = Time();
    track_edge *edge;

    if (train->lastSensorTick >= 0 && train->transition.state == CONST_SP) {
        /* 80% of original speed, 20% of new */
        Log("current micro/tick: %d, distToNextSensor: %d, tick: %d, lastSensorTick: %d",
            train->microPerTick, train->distToNextSensor, tick, train->lastSensorTick);
        train->microPerTick = ( (train->microPerTick * 4) + (train->distToNextSensor * 1000 / (tick - train->lastSensorTick)) ) / 5;
    }

    if (train->transition.stopping_distance != 0) {
        Log("(Timestamp %d) Train %u: sensor %s trip, distToNextSensor: %d, stopping_distance: %d",
            tick, train->id, d(sensor).name, train->distToNextSensor, train->transition.stopping_distance);
    }

    train->transition.stopping_distance = MAX(0, (int) (train->transition.stopping_distance - train->distToNextSensor));
    train->lastUpdateTick = tick;
    train->lastSensorTick = tick;
    train->distSinceLastSensor = 0;
    train->distSinceLastNode = 0;

    edge = getNextEdge(sensor);
    if (edge != NULL) {
        train->lastSensor = sensor;
        train->currentEdge = edge;
    }

    track_node *resvHead;
    while (true)  {
        // free reservation up until the sensor that was tripped
        resvHead = peek_head_resv(&(train->resv));
        if (resvHead == NULL || resvHead == sensor) {
            break;
        }

        freeHeadResv(train);

        // if the path exists, progress path as well
        if (train->path && resvHead == *(train->path)) {
            Log("Train %u: Progressing path %s", train->id, d(train->path[0]).name);
            train->path += 1;
            train->pathNodeRem -= 1;
        }
    }

    Log("Train %u: Finished freeing, head of path: %s", train->id, (train->path ? train->path[0]->name : "NULL"));
    if (train->path && peek_head_resv(&(train->resv)) == *(train->path) && train->pathNodeRem > 0) {
        train->path += 1;
        train->pathNodeRem -= 1;
    }

    updateNextSensor(train);

    int prevPathRemaining = train->pathRemain;
    train->pathRemain = pathRemaining(train);
    Log("Train %u: Recompute path remaining: old %d, new %d", train->id, prevPathRemaining, train->pathRemain);

    // reserve additional track if necessary
    int toResvDist = -1;

    if (train->transition.state == CONST_SP || train->transition.state == ACCEL) {
        // still moving (or will be moving)
        // Technically this should subtract distance between train and end of its edge..
        toResvDist = RESV_DIST(MAX(train->stoppingDist, train->distToNextSensor));
        toResvDist += train->distSinceLastNode - train->currentEdge->dist;
        Log("\n\nComputing resv dist: stopping dist %d, edge dist %d, distToNextSensor %d, toResvDist %d",
                train->stoppingDist, train->currentEdge->dist, train->distToNextSensor, toResvDist);
    } else if (train->transition.state == DECEL && train->transition.stopping_distance > d(train->currentEdge).dist) {
        // stopping but has enough stop distance to move over current edge
        // toResvDist = train->transition.stopping_distance - train->currentEdge->dist;
        toResvDist = train->transition.stopping_distance;
        Log("\n\nComputing resv dist: transition stop dist: %d, current edge dist: %d, toResvDist: %d",
                train->transition.stopping_distance, train->currentEdge->dist, toResvDist);
    }

    if (toResvDist > 0) {
        // if we do reserve more, remove the node that was just passed
        ASSERT(resvHead != NULL, "SensorTrip: Expected sensor(%s) to be reserved but it was not", d(sensor).name);
        freeHeadResv(train);

        track_node *lastResv = peek_back_resv(&(train->resv));
        train->resv.extraResvDist = -reserveTrack(train, toResvDist);
        Log("extra after resv dist %d\n", train->resv.extraResvDist);
        if (train->resv.extraResvDist < 0) {
            /* a collision has occured */
            error("Collision detected on train %d, stopping", train->id);
            setTrainSpeed(train, 0);
            train->gotoResult = GOTO_REROUTE;
        }

        execPath(train, lastResv);
    }
}

static bool trainMissSensor(Train_t *train) {
    return (train->distSinceLastNode >= d(train->currentEdge).dist + MAX_NODE_OFFSET
            && d(d(train->currentEdge).dest).type != NODE_EXIT);
}

static void distTraverse(Train_t *train, int numSensorTraverse) {
    track_edge *edge;
    while (train->distSinceLastNode > train->currentEdge->dist) {
        /*
        debug("Train %u: edge: %s -> %s, dist trav with %d > %d, head of resv %s",
            train->id, train->currentEdge->src->name, train->currentEdge->dest->name,
            train->distSinceLastNode, train->currentEdge->dist, d(peek_head_resv(&(train->resv))).name);
        */

        if (train->nextSensor == NULL || (train->currentEdge->dest == train->nextSensor && numSensorTraverse-- <= 0)) {
            // TODO: this is kind of concerning...
            break;
        }

        if (train->currentEdge->dest != peek_head_resv(&(train->resv))) {
            error("WARNING: Traversed node(%s) != reserved node(%s)",
                   d(train->currentEdge->dest).name, d(peek_head_resv(&(train->resv))).name);
        } else {
            if (train->transition.state == ACCEL || train->transition.state == CONST_SP) {
                // only free when we are moving forward
                freeHeadResv(train);
            }
        }

        if (train->path != NULL && train->pathRemain > 0) {
            if (train->path[0] != train->currentEdge->dest) {
                error("WARNING: Train %u: Traversed node (%s) != path node %s", train->id,
                      train->currentEdge->dest->name, train->path[0]->name);
            }
            train->path++;
            train->pathNodeRem--;
        }

        train->distSinceLastNode -= train->currentEdge->dist;

        if ((edge = getNextEdge(train->currentEdge->dest))) {
            train->currentEdge = edge;
        } else {
            ASSERT(false, "distTraverse hit null edge");
        }
    }
}


static void updateLocation(Train_t *train) {
    int ticks, numTraverse, traveledDist;
    bool firstStop = false;

    traveledDist = 0;
    numTraverse = 0;
    ticks = Time();

    if (train->transition.state == ACCEL || train->transition.state == DECEL) {
        // compute the time spent transition so far
        int32_t timeTransition = ticks - train->transition.time_issued;

        // detect if we've stopped our transition acceleration/deceleration state
        if (timeTransition >= train->transition.num_ticks) {
            train->microPerTick = train->transition.end_velo;;
            traveledDist = train->transition.stopping_distance;
            train->transition.state = (train->speed > 0 ? CONST_SP : STOP);
            firstStop = (train->transition.state == STOP);

        } else if (train->transition.state == ACCEL) {
            // TODO: update traveledDist here...?
            train->microPerTick = train->transition.end_velo * timeTransition / train->transition.num_ticks;
        }
    } else {
        traveledDist = (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    train->distSinceLastSensor += traveledDist;
    train->distSinceLastNode += traveledDist;
    train->resv.extraResvDist -= traveledDist;
    train->lastUpdateTick = ticks;

    if (traveledDist != 0) {
        distTraverse(train, 0);
        if (train->path && train->pathNodeRem > 0) {
            train->pathRemain -= traveledDist;
        }
    }

    if (firstStop == true) {
        PTL(train);
        freeResvOnStop(train);
    }

    // out of resv distance and still moving
    if (train->resv.extraResvDist < 0
         && (train->transition.state == CONST_SP || train->transition.state == ACCEL)) {

        track_node *lastResv = peek_back_resv(&(train->resv));

        Log("\n\nComputing resv dist: stopping dist %d, edge dist %d, distSinceLastNode %d past %s",
                train->stoppingDist, train->currentEdge->dist, train->distSinceLastNode, train->currentEdge->src->name);

        // Technically this should subtract distance between train and end of its edge..
        int resvDist = RESV_DIST(MAX(train->stoppingDist, train->distToNextSensor));
        resvDist += train->distSinceLastNode - train->currentEdge->dist;

        Log("calling reserveTrack, asking for %d past end of dest\n", resvDist);
        train->resv.extraResvDist = -reserveTrack(train, resvDist);
        Log("Extra resv dist after resv: %d\n", train->resv.extraResvDist);
        if (train->resv.extraResvDist < 0) {
            /* a collision has occured */
            error("Collision detected on train %d, stopping", train->id);
            setTrainSpeed(train, 0);
            train->gotoResult = GOTO_REROUTE;
        }

        if (lastResv != peek_back_resv(&(train->resv))) {
            execPath(train, lastResv);
        }
    }

    if (train->path && train->pathRemain < train->stoppingDist + STOP_BUFFER_MM && train->speed != 0) {
        train->path = NULL;
        debug("Train %u: Critical path remaining reached at %d for stopping dist %d, train loc %d after %s, stopping",
              train->id, train->pathRemain, train->stoppingDist, train->distSinceLastNode, train->currentEdge->src->name);
        train->gotoResult = GOTO_COMPLETE;
        setTrainSpeed(train, 0);
    }

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

    if ((status = WaitOnSensorN(wait)) < 0) {
        error("SensorCourier: Error: WaitOnSensorN returned %d", status);
    }
    Send(train, NULL, 0, NULL, 0);
}


// return: id of time-based courier for when this is expected to hit
static void waitOnNextTarget(Train_t *train, int *sensorCourier, int *waitingSensor, int *timeoutCourier) {
    TrainMessage_t msg1;

    if (*timeoutCourier >= 0) {
        Destroy(*timeoutCourier);
        *timeoutCourier = -1;
    }

    if (*waitingSensor >= 0) {
        FreeSensor(*waitingSensor);
        *waitingSensor = -1;
    }

    if (*sensorCourier >= 0) {
        Destroy(*sensorCourier);
        *sensorCourier = -1;
    }

    // potentially need to call updateNextSensor here? only if track state changed between
    // last sensor trip and this
    if (train->nextSensor != NULL && train->nextSensor->reservedBy == train->id) {
        *waitingSensor = train->nextSensor->num;
        *sensorCourier = Create(4, SensorCourierTask);
        msg1.type = TRM_SENSOR_WAIT;
        msg1.arg0 = train->nextSensor->num;
        Log("Train %u: waiting on sensor %s with courier tid %d", train->id, train->nextSensor->name, *sensorCourier);
        Send(*sensorCourier, &msg1, sizeof(msg1), NULL, 0);

        int timeout = -1;
        if (timeout > 0) {
            // debug("Train %u: timeout courier with delay %u ticks", train->id, timeout);
            *timeoutCourier = CourierDelay(timeout, DELAY_PRIORITY);
        }
        Log("waiting at %d past %s for sensor %s at %d mm away, going at speed %d, with timeout %d (tid %d)",
            train->distSinceLastNode, (train->currentEdge && train->currentEdge->src ? train->currentEdge->src->name : "NULL"),
            train->nextSensor->name, train->distToNextSensor, train->microPerTick, timeout, *timeoutCourier);
    } else {
        Log("train %d failed to wait on %s (resv by %d)", train->id,
            (train->nextSensor ? train->nextSensor->name : "NULL"), (train->nextSensor ? train->nextSensor->reservedBy : -1));
    }

    if (train->nextSensor && train->nextSensor->reservedBy != train->id) {
        error("WARNING: Train %u next sensor %s not reserved by it", train->id, train->nextSensor->name);
    }
}


static void setTrainSpeed(Train_t *train, int speed) {
    char buf[2];

    Log("Train %u: speed %d -> %d, train at %d past %s", train->id,
        train->speed, speed, train->distSinceLastNode, d(d(train->currentEdge).src).name);

    if (speed != train->speed) {
        int tick = Time();

        if (train->speed > speed) {
            train->transition.state = DECEL;
        } else if (train->speed < speed) {
            train->transition.state = ACCEL;
        }

        train->transition.num_ticks = getTransitionTicks(train->id, train->speed, speed);
        train->transition.end_velo = getTrainVelocity(train->id, speed);

        train->transition.time_issued = tick;
        train->lastUpdateTick = tick;
        train->lastSensorTick = -1;
        if (speed == 0) {
            updateNextSensor(train);
            PTL(train);
            train->transition.stopping_distance = train->stoppingDist;
        } else {
            train->transition.stopping_distance = 0;
        }
        train->speed = speed;
        train->stoppingDist = getStoppingDistance(train->id, train->speed, speed);
    }
    buf[0] = train->speed + train->aux;
    buf[1] = train->id;
    trnputs(buf, 2);
}

static int trainDir(Train_t *train) {
    if (train->speed != 0 || train->transition.state != STOP) {
        error("Train %u: Cannot reverse while moving, ticks: %u, called by: %u",
                train->id, Time());
        return -1;
    }

    ASSERT(train->currentEdge, "Current edge is NULL pre-rev");

    if (train->currentEdge->dist < train->distSinceLastNode) {
        error("WARNING: currentEdge->dist %d less than distSinceLastNode %d", train->currentEdge->dist, train->distSinceLastNode);
    }
    Log("Train %u: Rev: initial %d from %s", train->id, train->distSinceLastNode, train->currentEdge->src->name);

    /* recompute distances from sensors and nodes */
    train->currentEdge = train->currentEdge->reverse;
    train->distSinceLastNode = MAX(0, train->currentEdge->dist - train->distSinceLastNode);

    ASSERT(train->currentEdge, "Current edge is NULL post-rev");

    Log("Rev: now %d from %s", train->distSinceLastNode, train->currentEdge->src->name);
    if (train->currentEdge->src->type == NODE_BRANCH) {
        ASSERT(flipSwitch(train->currentEdge->src, train->currentEdge->dest) == 0,
               "Flipping switch on same edge, this should never fail");
    }

    updateNextSensor(train);

    Log("Updated sensor to %s", (train->nextSensor ? train->nextSensor->name : "NULL"));
    if (DispatchReserveTrack(train->id, &(train->currentEdge->dest), 1) == 1) {
        freeHeadResv(train);
        push_back_resv(&(train->resv), train->currentEdge->dest);
    } else {
        // WTF? we should already have this (alas the reverse one) reserved...
        error("Train %u: Reverse reservation failed for %s", train->id, train->currentEdge->dest->name);
        return -2;
    }
    Log("rev success");

    char command[2];
    command[0] = TRAIN_AUX_REVERSE;
    command[1] = train->id;
    trnputs(command, 2);

    if (train->pickupOffset == PICKUP_FRONT) {
        train->pickupOffset = PICKUP_BACK;
    } else if (train->pickupOffset == PICKUP_BACK) {
        train->pickupOffset = PICKUP_FRONT;
    } else {
        Panic("Pickup offset is %d, which is neither PICKUP_FRONT (%d) or PICKUP_BACK (%d)",
                train->pickupOffset, PICKUP_FRONT, PICKUP_BACK);
    }

    return 0;
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
    train->distOffset = 0;
    train->pathRemain = 0;
    train->pathNodeRem = 0;
    train->gotoResult = GOTO_NONE;

    train->pickupOffset = PICKUP_FRONT;

    // reservation array
    int i;
    for (i = 0; i < (1 << RESV_BUF_BITS); ++i) {
        train->resv.arr[i] = 0;
    }
    train->resv.head = 0;
    train->resv.tail = 0;
    train->resv.extraResvDist = 0;

    /* initialize transition */
    train->transition.state = STOP;
    train->transition.num_ticks = 0;
    train->transition.end_velo = 0;
    train->transition.time_issued = 0;
    train->transition.stopping_distance = 0;

    // if train moved far enough past sensor, update it here
    // similar to updateLocation's update but this lets us get our initial reservation
    // whereas that guy also tries to free
    int sanity = 8;
    track_edge *edge;
    while (train->distSinceLastNode > train->currentEdge->dist) {
        train->distSinceLastNode -= train->currentEdge->dist;
        edge = getNextEdge(train->currentEdge->dest);

        if ((edge = getNextEdge(train->currentEdge->dest)) == NULL) {
            break;
        }

        train->currentEdge = edge;

        if (sanity -- < 0) {
            Panic("Failed to finish traversal after 8 tries, dist %d remaining", train->distSinceLastNode);
        }
    }

    ASSERT(train->currentEdge != NULL, "Train current edge is NULL WAT");

    updateNextSensor(train);

    // reserve your initial sensor
    int numSuccessResv = DispatchReserveTrack(train->id, &(train->currentEdge->src), 1);
    ASSERT(numSuccessResv, "Train initial sensor reservation failed");
    push_back_resv(&(train->resv), train->currentEdge->src);

    if (train->currentEdge->dist < RESV_BUFFER + train->distSinceLastNode) {
        numSuccessResv = DispatchReserveTrack(train->id, &(train->currentEdge->dest), 1);
        ASSERT(numSuccessResv, "Train initial sensor reservation failed");
        push_back_resv(&(train->resv), train->currentEdge->dest);
    }
}


static void TrainReverseCourier() {
    int callee, status;
    TrainMessage_t request, message = {.type = TRM_DIR};

    Receive(&callee, &request, sizeof(request));
    status = 0;
    if (callee == MyParentTid()) {
        Reply(callee, &status, sizeof(status));
        message.type = TRM_SPEED;
        message.arg0 = 0;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        /* adjust by five(5) to compensate for the clock edge */
        Delay(getTransitionTicks(request.tr, request.arg0, 0) + STOP_BUFFER_MM);
        message.type = TRM_DIR;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
        Delay(20);
        message.type = TRM_SPEED;
        message.arg0 = request.arg0;
        Send(callee, &message, sizeof(message), &status, sizeof(status));
    } else {
        status = -1;
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}


static void TrainTask() {
    Train_t train;
    char command[2];
    TrainMessage_t request;
    unsigned int dispatcher, speed;
    int status, bytes, callee, gotoBlocked;
    int sensorCourier, locationTimer, reverseCourier, waitingSensor, timeoutCourier;
    int numSensorMissed = 0;

    int shortMvStop, shortMvDone;

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
    gotoBlocked = -1;
    timeoutCourier = -1;
    shortMvStop = -1;
    shortMvDone = -1;
    locationTimer = Create(2, TrainTimer);

    while (true) {
        if ((bytes = Receive(&callee, &request, sizeof(request))) < 0) {
            error("TrainTask: Error: Received %d from %d", bytes, callee);
            status = -1;
            Reply(callee, &status, sizeof(status));
            continue;
        }

        // 1st block - special case for couriers that we create and know the tid of
        // no message type, the context of them replying is enough

        status = 0;
        if (callee == sensorCourier || callee == timeoutCourier) {
            Log("Train %u: sensor %s trip, train thinks it's at %d from %s",
                train.id, train.nextSensor->name, train.distSinceLastNode, d(train.currentEdge->src).name);

            if (callee == timeoutCourier) {
                Log("Train %u: Timeout on sensor %s", train.id,
                    (train.nextSensor != NULL ? d(train.nextSensor).name : "NULL"));
                if (train.nextSensor != NULL && ++numSensorMissed > SENSOR_MISS_THRESHOLD) {
                    Log("<<<<Too many timeouts, stopping>>>>");
                    numSensorMissed = 0;
                    setTrainSpeed(&train, 0);
                    train.gotoResult = GOTO_LOST;
                    continue;
                }
            } else {

                numSensorMissed = 0;
            }

            if (train.nextSensor != NULL) {
                // TODO: on timeout, we need to take in account how much this actually travelled?
                sensorTrip(&train, train.nextSensor);
                if (train.nextSensor) {
                    Log("Sensor trip - Next sensor: %s", d(train.nextSensor).name);
                    waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                    train.gotoResult = GOTO_COMPLETE;
                } else {
                    Log("Sensor trip - next sensor NULL, should reverse on stop");
                }
            }
            CalibrationSnapshot(&train);
            Log("Train %u: Sensor trip - done", train.id);
            continue;
        } else if (callee == shortMvStop) {
            trainSpeed(train.id, 0);
            shortMvStop = -1;
            Destroy(callee);
            debug("===shortMvStop===");
            PTL(&train);
            continue;
        } else if (callee == shortMvDone) {
            shortMvDone = -1;
            Destroy(callee);
            // set this to true so next updateLocation call will push us to the final location
            debug("===shortMvDone===");
            PTL(&train);
            train.distSinceLastNode += train.transition.stopping_distance;
            distTraverse(&train, 1);
            if (trainMissSensor(&train)) {
                error("Lost after short move?!?!?!?!?!?!?!?!");
                train.gotoResult = GOTO_LOST;
            } else {
                freeResvOnStop(&train);
                train.gotoResult = GOTO_COMPLETE;
            }
            PTL(&train);
            continue;
        } else if (callee == locationTimer) {
            updateLocation(&train);

            if (train.speed != 0
                    && train.distSinceLastNode > train.currentEdge->dist + DEAD_SENSOR_BUFFER
                    && train.currentEdge->dest == train.nextSensor) {

                notice("Sensor trip timeout on: %s", d(train.nextSensor).name);

                // moving train, on edge with sensor being dest and overshot the edge length
                if (++numSensorMissed > SENSOR_MISS_THRESHOLD) {
                    error("<<<<Too many sensor miss, stopping>>>>");
                    train.gotoResult = GOTO_LOST;
                    setTrainSpeed(&train, 0);
                } else {
                    sensorTrip(&train, train.nextSensor);
                    if (train.nextSensor) {
                        Log("Train %u: Sensor trip timeout - Next sensor: %s", train.id, d(train.nextSensor).name);
                        waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                        train.gotoResult = GOTO_COMPLETE;
                    } else {
                        Log("Train %u: Sensor trip timeout - next sensor NULL, should reverse on stop", train.id);
                        setTrainSpeed(&train, 0);
                    }
                }
            }

            if (train.gotoResult == GOTO_LOST && gotoBlocked != -1) {
                // Totally lost
                Log("Train %u: Lost!!!!!! shit shit shit", train.id);
                Reply(gotoBlocked, &(train.gotoResult), sizeof(train.gotoResult));
                gotoBlocked = -1;
            }

            if (train.speed == 0 && gotoBlocked != -1 && train.transition.state == STOP && shortMvDone == -1) {
                debug("===Train done pathing===");

                // Done pathing
                if (trainMissSensor(&train)) {
                    debug("Lost after pathing?!?!?!?!!!?!!?!1111111one");
                    train.gotoResult = GOTO_LOST;
                }

                Log("Train %u: Finished pathing, replying to conductor (%d) with result %d",
                        train.id, gotoBlocked, train.gotoResult);
                Reply(gotoBlocked, &(train.gotoResult), sizeof(train.gotoResult));
                gotoBlocked = -1;
            }

            Reply(callee, NULL, 0);
            continue;
        }

        // 2nd block - external task request something of us, message actually matters

        switch (request.type) {
            case TRM_DELETE:
                status = 0;
                if (reverseCourier != -1) {
                    Destroy(reverseCourier);
                    ++status;
                }
                if (sensorCourier != -1) {
                    FreeSensor(waitingSensor);
                    Destroy(sensorCourier);
                    ++status;
                }
                if (timeoutCourier != -1) {
                    Destroy(timeoutCourier);
                    ++status;
                }
                if (shortMvStop != -1) {
                    Destroy(shortMvStop);
                    ++status;
                }
                if (shortMvDone != -1) {
                    Destroy(shortMvDone);
                    ++status;
                }

                /* free any nodes we have reserved */
                while (peek_head_resv(&(train.resv)) != NULL) {
                    freeHeadResv(&train);
                }

                Destroy(locationTimer);
                status++;
                Reply(callee, &status, sizeof(status));
                notice("Train %u: Deleted (Tid %u)", train.id, MyTid());
                PTL(&train);
                clearTrainSnapshot(train.id);
                Reply(callee, &status, sizeof(status));
                Exit();
                break;
            case TRM_GOTO_AFTER:
                /* blocks the calling task until we arrive at our destination */
                gotoBlocked = callee;
                callee = -1;
                const int stoppingDist = getStoppingDistance(train.id, GOTO_SPEED, 0);

                if (request.arg0 == NULL) {
                    train.path = NULL;
                    train.pathNodeRem = 0;
                    train.pathRemain = request.arg2;
                } else {
                    train.path = (track_node**)request.arg0;
                    train.pathNodeRem = request.arg1;
                    train.distOffset = request.arg2;
                    train.pathRemain = pathRemaining(&train);
                }

                updateNextSensor(&train);

                int resvDist = 0;
                if (train.pathRemain < (stoppingDist + (stoppingDist >> 1))) {
                    // short move
                    resvDist = train.pathRemain;
                } else {
                    resvDist = RESV_DIST(MAX(stoppingDist, train.distToNextSensor)) + RESV_BUFFER;
                }

                train.resv.extraResvDist = -reserveTrack(&train, resvDist);

                if (train.resv.extraResvDist >= 0) {
                    /* remove the previously reserved nodes */
                    while (peek_head_resv(&(train.resv)) != train.currentEdge->dest) {
                        freeHeadResv(&train);
                    }
                    execPath(&train, NULL);
                } else {
                    // resv failed, undo
                    while (size_resv(&(train.resv)) > 1) {
                        freeTailResv(&train);
                    }
                    train.gotoResult = GOTO_REROUTE;
                    break;
                }

                Log("Train %u: Move from edge %d past %s -> destination %s", train.id, train.distSinceLastNode,
                    d(train.currentEdge).src->name, (train.path ? d(train.path[train.pathNodeRem - 1]).name : "NULL"));
                Log("Train %u: resvHead = %s, path[0] = %s", train.id, d(peek_head_resv(&(train.resv))).name,
                    (train.path ? d(train.path[0]).name : "NULL"));
                PTL(&train);

                // 2x stopping dist using short moves will probably break shit
                // TODO: use piece-wise function
                if (train.pathRemain < (stoppingDist + (stoppingDist >> 1))) {
                    int delayTime = shortmoves(train.id, GOTO_SPEED, train.pathRemain);
                    int time = Time();

                    Log("(Timestamp %d): Train %u: Short move with delay %d ticks and %d path nodes to cover %d mm",
                           time, train.id, delayTime, train.pathNodeRem, train.pathRemain);

                    trainSpeed(train.id, GOTO_SPEED);
                    train.transition.stopping_distance = train.pathRemain;

                    ASSERT((shortMvStop = CourierDelay(delayTime, DELAY_PRIORITY)) >= 0,
                            "shortMvStop CourierDelay returned negative value %d!", shortMvStop);
                    ASSERT((shortMvDone = CourierDelay(delayTime << 1, DELAY_PRIORITY)) >= 0,
                            "shortMvDone CourierDelay returned negative value %d!", shortMvStop);

                    Log("Train %u: created shortMvStop {%d} and shortMvDone {%d}", train.id, shortMvStop, shortMvDone);
                } else {
                    // normal move
                    setTrainSpeed(&train, GOTO_SPEED);
                }
                waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                break;
            case TRM_SPEED:
                speed = request.arg0;
                if (speed > TRAIN_MAX_SPEED) {
                    status = INVALID_SPEED;
                    Reply(callee, &status, sizeof(status));
                } else {
                    status = 1;
                    Reply(callee, &status, sizeof(status));
                }

                updateNextSensor(&train);

                if (speed == 0) {
                    setTrainSpeed(&train, speed);
                } else {
                    train.resv.extraResvDist = -reserveTrack(&train, RESV_DIST(MAX(train.stoppingDist, train.distToNextSensor)));
                    if (train.resv.extraResvDist >= 0) {
                        /* remove the previously reserved nodes */
                        while (peek_head_resv(&(train.resv)) != train.currentEdge->dest) {
                            freeHeadResv(&train);
                        }
                        dump_resv(&(train.resv));
                        setTrainSpeed(&train, speed);
                        waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                    } else {
                        // resv failed, undo
                        while (size_resv(&(train.resv)) > 1) {
                            freeTailResv(&train);
                        }
                    }
                }
                break;
            case TRM_AUX:
                if (request.arg0 == TRAIN_LIGHT_OFFSET && train.aux == TRAIN_LIGHT_OFFSET) {
                    train.aux = 0;
                } else if (request.arg0 == TRAIN_HORN_OFFSET && train.aux == TRAIN_HORN_OFFSET) {
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
                status = trainDir(&train);
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_RV:
                reverseCourier = Create(8, TrainReverseCourier);
                request.type = TRM_RV;
                request.tr = train.id;
                request.arg0 = train.speed;
                Send(reverseCourier, &request, sizeof(request), &status, sizeof(status));
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_GET_LOCATION:
                updateLocation(&train);
                request.arg0 = (int)train.currentEdge->src;
                request.arg1 = train.distSinceLastNode;
                Reply(callee, &request, sizeof(request));
                break;
            case TRM_ROUTE_COMPLETE:
                if (train.gotoResult == GOTO_COMPLETE || train.gotoResult == GOTO_NONE) {
                    status = 1;
                } else {
                    status = 0;
                }
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_GET_EDGE:
                updateLocation(&train);
                request.arg0 = (int)train.currentEdge;
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
