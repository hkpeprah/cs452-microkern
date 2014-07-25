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

#define GOTO_SPEED             10
#define STOP_BUFFER_MM         15
#define RESV_BUF_BITS          4
#define PICKUP_FRONT           165
#define PICKUP_BACK            45
#define RESV_MOD               ((1 << RESV_BUF_BITS) - 1)
#define RESV_DIST(req_dist)    ((req_dist * 5) >> 2)
#define SENSOR_MISS_THRESHOLD  2


typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
    unsigned int stopping_distance;
    int shortmove;
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
    TRM_DELETE
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
static track_node *peek_any_Resv(TrainResv_t *resv, uint32_t index);
static track_node *peek_back_Resv(TrainResv_t *resv);
static track_node *peek_head_Resv(TrainResv_t *resv);


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

    Send(tid, &msg, sizeof(msg), &msg, sizeof(msg));
    return (track_edge*)msg.arg0;
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
    snapshot.headResv = ((n = peek_head_Resv(&(train->resv))) ? n->name : "NULL");
    snapshot.tailResv = ((n = peek_back_Resv(&(train->resv))) ? n->name : "NULL");
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

static uint32_t size_Resv(TrainResv_t *resv) {
    uint32_t size = (resv->tail - resv->head) & RESV_MOD;
    return size;
}


static void dump_Resv(TrainResv_t *resv) {
    // should be debugging only
    int i;
    Log("DUMPING RESV: | ");
    for (i = 0; i < size_Resv(resv); ++i) {
        Log(" %s ", peek_any_Resv(resv, i)->name);
    }
    Log(" |\n");
}

static track_node *pop_back_Resv(TrainResv_t *resv) {
    if (size_Resv(resv) <= 0) {
        return NULL;
    }
    resv->tail = (resv->tail - 1) & RESV_MOD;
    return resv->arr[resv->tail];
}

static track_node *pop_head_Resv(TrainResv_t *resv) {
    if (size_Resv(resv) <= 0) {
        return NULL;
    }
    int ind = resv->head;
    resv->head = (resv->head + 1) & RESV_MOD;
    return resv->arr[ind];
}


static track_node *peek_back_Resv(TrainResv_t *resv) {
    int ind = (resv->tail - 1) & RESV_MOD;
    return (size_Resv(resv) > 0) ? resv->arr[ind] : NULL;
}


static track_node *peek_head_Resv(TrainResv_t *resv) {
    return (size_Resv(resv) > 0) ? resv->arr[resv->head] : NULL;
}


static track_node *peek_any_Resv(TrainResv_t *resv, uint32_t index) {
    if (index >= size_Resv(resv)) {
        // error("Index %d out of bounds, head %u, tail %u", index, resv->head, resv->tail);
        return NULL;
    }

    return resv->arr[(resv->head + index) & RESV_MOD];
}


static void push_back_Resv(TrainResv_t *resv, track_node *node) {
    track_node *tail = peek_back_Resv(resv);

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


static int find_Resv(TrainResv_t *resv, track_node *node) {
    int i;
    for (i = 0; i < size_Resv(resv); ++i) {
        if (peek_any_Resv(resv, i) == node) {
            return i;
        }
    }
    return -1;
}


static void freeHeadResv(Train_t *train) {
    track_node *toFree = pop_head_Resv(&(train->resv));
    if (toFree) {
        int freed = DispatchReleaseTrack(train->id, &toFree, 1);
        ASSERT(freed == 1, "Failed to free head of resv node %s (reserved by %u)",
               toFree->name, toFree->reservedBy);
        Log("Freed head %s\n", toFree->name);
    } else {
        Log("WARNING: freeHeadResv called on null queue\n");
    }
}


static void freeTailResv(Train_t *train) {
    track_node *toFree = pop_back_Resv(&(train->resv));
    if (toFree) {
        int freed = DispatchReleaseTrack(train->id, &toFree, 1);
        ASSERT(freed == 1, "Failed to free tail of resv node %s (reserved by %u)",
               toFree->name, toFree->reservedBy);
        Log("Freed tail %s\n", toFree->name);
    } else {
        Log("WARNING: freeTailResv called on null queue\n");
    }
}


static void updateNextSensor(Train_t *train) {
    int i, nextNodeDist;
    track_edge *edge;
    track_node *node, **path;

    i = 0;
    node = d(train->currentEdge).src;
    edge = NULL;
    train->distToNextSensor = 0;
    path = train->path;

    if (train->path != NULL) {
        train->distToNextSensor += train->currentEdge->dist - train->distSinceLastNode;
    }

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
                train->nextSensor = NULL;
                return;
            } else {
                train->distToNextSensor += d(edge).dist;
                node = d(edge).dest;
            }
        }
    } while (node != NULL && node->type != NODE_SENSOR);

    if ((train->nextSensor = node)) {
        Log("UpdateNextSensor, Train %u -> %s with dist %d\n",
            train->id, node->name, train->distToNextSensor);
    }
}


static uint32_t pathRemaining(const Train_t *train) {
    /* computes the distance to the end of a path */
    int i, nextNodeDist;
    track_node **path = train->path;
    uint32_t pathDist = train->currentEdge->dist - train->distSinceLastNode + train->distOffset + train->pickupOffset;

    Log("path computation: edge total dist {%d}, train at {%d} past src {%s}\n",
        train->currentEdge->dist, train->distSinceLastNode, train->currentEdge->src->name);

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
    while ((node = peek_head_Resv(&train->resv))) {
        if (node == train->currentEdge->src || node == train->currentEdge->dest) {
            break;
        }
        freeHeadResv(train);
    }

    while ((node = peek_back_Resv(&train->resv))) {
        if (node == train->currentEdge->src || node == train->currentEdge->dest) {
            break;
        }
        freeTailResv(train);
    }

    Log("After freeResvOnStop @ %s\n", train->currentEdge->src->name);
    dump_Resv(&(train->resv));
}


// request path
// returns amount of dist left to reserve (can be negative, ie. there's extra)
static int reserveTrack(Train_t *train, int resvDist) {
    int numResv, numSuccessResv;
    track_node *resvArr[RESV_MOD] = {0};
    track_node **toResv = resvArr;

    if (train->path != NULL) {
        /* train has a path, need to reserve along it */

        if (train->currentEdge->dest != train->path[0] && find_Resv(&(train->resv), train->path[0]) == -1) {
            // oh no! we're a bit lost! do a short path find to train->path[0]
            // if it still fails, give up and ask to be rerouted
            Log("overshot path, edge dest %s but path[0] %s\n", train->currentEdge->dest->name, train->path[0]->name);
            dump_Resv(&(train->resv));

            track_node *overshotComp[4];
            int ospathLen;
            uint32_t totalDistance;

            if ((ospathLen = findPath(train->id, train->currentEdge, train->path[0], overshotComp, 4, &totalDistance)) < 0) {
                // fail, return positive number
                Log("findPath failed on overshot correction, stopping\n");
                return 1;
            }

            // otherwise, we found a way back to the head of path!
            int i, numSuccessResv = DispatchReserveTrack(train->id, overshotComp, ospathLen);
            for (i = 0; i < numSuccessResv; ++i) {
                push_back_Resv(&(train->resv), overshotComp[i]);
            }

            if (ospathLen != numSuccessResv) {
                // ... but failed to reserve it
                Log("reserve failed on overshot correction, stopping\n");
                return 1;
            }

            Log("Resv overshot path of length %d\n", totalDistance);
        }

        toResv = train->path;
        numResv = train->pathNodeRem;

    } else {
        /* train does not have a path, find nodes to reserve */
        numResv = 0;
        int distRemaining = resvDist;
        track_node *node = train->currentEdge->dest;
        track_edge *edge = getNextEdge(node);

        while (node && edge && distRemaining > 0) {
            toResv[numResv++] = node;
            distRemaining -= edge->dist;
            node = edge->dest;
            edge = getNextEdge(node);
        }

        ASSERT(node != NULL, "reserveTrack: Train %u: node should not be NULL", train->id);
        /* train overshot, so there is extract stopping distance */
        toResv[numResv++] = node;
        if (!edge) {
            // edge null -> exit! has to stop now
            return 1;
        }
        distRemaining -= d(edge).dist;

        if (numResv <= 0) {
            return resvDist;
        }

        ASSERT(numResv < RESV_MOD, "reserveTrack: Train %u: Overshot number of nodes to reserve", train->id);
    }
    numSuccessResv = DispatchReserveTrackDist(train->id, toResv, numResv, &resvDist);

    track_edge *lastResvEdge = NULL;
    track_node *lastResvNode = NULL;

    // find the tail before reserving - this is so that if only 1 node is resvd
    // then we have to subtract the edge distance ourself
    if ((lastResvNode = peek_back_Resv(&(train->resv)))) {
        lastResvEdge = getNextEdge(lastResvNode);
        if (numSuccessResv > 1 && lastResvEdge && lastResvEdge->dest == toResv[numResv - 1]) {
            resvDist -= lastResvEdge->dist;
        }
    }

    int i, osResvBase;
    track_node *nextResv = NULL;

    if ((osResvBase = find_Resv(&(train->resv), toResv[0])) < 0) {
        osResvBase = 0;
    }

    for (i = 0; i < MIN(numSuccessResv, numResv); ++i) {
        nextResv = toResv[i];
        if (peek_any_Resv(&(train->resv), osResvBase + i) == nextResv) {
            continue;
        }

        ASSERT(nextResv->reservedBy == train->id,
               "Whoa, pushing back node %s reserved by %d on train %d", nextResv->name, nextResv->reservedBy, train->id);
        push_back_Resv(&(train->resv), nextResv);
    }

    for (; i < numSuccessResv; ++i) {
        nextResv = d(getNextEdge(nextResv)).dest;
        ASSERT(d(nextResv).reservedBy == train->id,
               "Whoa, pushing back node %s reserved by %d on train %d", nextResv->name, nextResv->reservedBy, train->id);
        push_back_Resv(&(train->resv), nextResv);
    }

    if (train->path && train->pathNodeRem <= numSuccessResv && resvDist >= 0) {
        // reserved up to the path's last node, return "good to go" value to indicate okay to go
        // and the path computation code will take care of stopping the train
        return -1;
    }

    dump_Resv(&(train->resv));

    return resvDist;
}


static void execPath(Train_t *train, track_node *lastExec) {
    track_node *current;
    track_node *next;
    int i;

    if (train->path == NULL) {
        return;
    }

    Log("execPath for train %u\n", train->id);

    if (train->currentEdge->dest != train->path[0]) {
        Log("WARNING: execPath: Train edge dest(%s) is not equal to start of path(%s)\n",
            train->currentEdge->dest->name, train->path[0]->name);
    }

    if (peek_head_Resv(&(train->resv)) != train->path[0]) {
        /* TODO: head of the train reservation is NULL, but path is not ? */
        Log("WARNING: execPath: Expected resv (%s) to be path[0] but got %s\n",
            d(peek_head_Resv(&(train->resv))).name, train->path[0]->name);
    }

    i = 0;
    do {
        current = peek_any_Resv(&(train->resv), i++);
    } while (lastExec && current != lastExec);

    Log("starting path exec at resv index %d node %s for train %u\n",
        i, d(current).name, train->id);

    int resvSize = size_Resv(&(train->resv));

    for (; i < resvSize; ++i) {
        next = peek_any_Resv(&(train->resv), i);

        if (d(current).type == NODE_BRANCH) {
            if (current->edge[DIR_STRAIGHT].dest == next) {
                trainSwitch(current->num, 'S');
                debug("%s -> 's'", current->name);
            } else if (current->edge[DIR_CURVED].dest == next) {
                trainSwitch(current->num, 'C');
                debug("%s -> 'c'", current->name);
                if (current->num == 156 || current->num == 154) {
                    trainSwitch(current->num - 1, 'S');
                }
            } else {
                error("execPath: Train %u: Error with path at %s to %s", train->id, current->name, d(next).name);
            }
        }
        current = next;
    }

    Log("exec path finished\n");
}


static void sensorTrip(Train_t *train, track_node *sensor) {
    int tick;
    track_edge *edge;

    /* if train->nextSensor is NULL, we've reached an exit node */
    if (train->nextSensor == NULL) {
        return;
    }

    ASSERT((train->nextSensor != NULL && sensor != NULL), "SensorTrip: NULL sensor");
    ASSERT(train->nextSensor == sensor, "SensorTrip: Expected sensor(%s) vs actual differ(%s)",
           train->nextSensor->name, sensor->name);
    ASSERT(sensor->type == NODE_SENSOR, "SensorTrip: Called with non-sensor: Type %d", sensor->type);

    tick = Time();
    if (train->lastSensorTick >= 0 && !train->transition.valid && train->speed != 0) {
        /* 80% of original speed, 20% of new */
        train->microPerTick = ( (train->microPerTick * 4) + (train->distToNextSensor * 1000 / (tick - train->lastSensorTick)) ) / 5;
    }

    train->transition.stopping_distance = MAX(0, (int) (train->transition.stopping_distance - train->distToNextSensor));

    Log("sensor %s trip, stopping_distance: %d\n", sensor->name, train->transition.stopping_distance);

    train->lastUpdateTick = tick;
    train->lastSensorTick = tick;
    train->distSinceLastSensor = 0;
    train->distSinceLastNode = 0;

    edge = getNextEdge(sensor);
    if (edge != NULL) {
        train->lastSensor = sensor;
        train->currentEdge = edge;
    }

    if (train->path != NULL) {
        while (*(train->path) != sensor && train->pathNodeRem > 0) {
            train->path++;
            train->pathNodeRem--;
        }

        if (train->pathNodeRem > 0) {
            train->path++;
            train->pathNodeRem--;
        }

        train->pathRemain = pathRemaining(train);
        debug("Train %u: Compute path: %d", train->id, train->pathRemain);
    }

    updateNextSensor(train);

    track_node *resvHead;
    while (true)  {
        resvHead = peek_head_Resv(&(train->resv));
        if (resvHead == NULL || resvHead == sensor) {
            break;
        }
        freeHeadResv(train);
    }

    if (train->speed != 0 || (train->transition.valid && (train->transition.dest_speed != 0)) ||
        train->transition.stopping_distance > d(train->currentEdge).dist) {
        ASSERT(resvHead != NULL, "SensorTrip: Expected sensor(%s) to be reserved but it was not", d(sensor).name);
        freeHeadResv(train);

        track_node *lastResv = peek_back_Resv(&(train->resv));
        train->resv.extraResvDist = -reserveTrack(train, MAX(train->distToNextSensor,
            RESV_DIST(train->stoppingDist - train->currentEdge->dist)));
        if (train->resv.extraResvDist < 0) {
            /* a collision has occured */
            Log("sensorTrip - Collision detected on train %d, stopping\n", train->id);
            setTrainSpeed(train, 0);
            train->gotoResult = GOTO_REROUTE;
        }
        execPath(train, lastResv);
    }
}


static void distTraverse(Train_t *train, bool traverseSensor) {
    track_edge *edge;
    while (train->distSinceLastNode > train->currentEdge->dist) {
        if (train->nextSensor == NULL || (train->currentEdge->dest == train->nextSensor && !traverseSensor)) {
            // TODO: this is kind of concerning...
            // error("updateLocation: Error: Model beat train to %s", train->nextSensor->name);
            break;
        }

        Log("edge: %s -> %s, dist trav with %d > %d, head of resv %s\n",
                train->currentEdge->src->name, train->currentEdge->dest->name,
                train->distSinceLastNode, train->currentEdge->dist, d(peek_head_Resv(&(train->resv))).name);

        if (train->currentEdge->dest != peek_head_Resv(&(train->resv))) {
            debug("WARNING: Traversed node(%s) != reserved node(%s)",
                    d(train->currentEdge->dest).name, d(peek_head_Resv(&(train->resv))).name);
        } else {
            if (train->speed != 0) {
                // only free when going at full speed
                freeHeadResv(train);
            }
        }

        if (train->path != NULL) {
            debug("Traversing path node %s", train->path[0]->name);
            train->path++;
            train->pathNodeRem--;
        }

        train->distSinceLastNode -= train->currentEdge->dist;

        if ((edge = getNextEdge(train->currentEdge->dest))) {
            train->currentEdge = edge;
        } else {
            ASSERT(false, "distTraverse hit null edge\n");
        }
    }
}


static void updateLocation(Train_t *train) {
    int ticks, numTraverse, traveledDist;
    bool fullStop = false;

    traveledDist = 0;
    numTraverse = 0;
    ticks = Time();

    if (train->transition.valid) {
        uint32_t start_speed = train->transition.start_speed;
        uint32_t stop_speed = train->transition.dest_speed;
        /* compute the time spent transition so far */
        int32_t timeTransition = ticks - train->transition.time_issued;
        int32_t srtmove = shortmoves_dist(train->id, stop_speed, timeTransition);
        /* adjust the traveled distance based on the previously travelled distance */
        if (timeTransition >= 20) {
            // traveledDist = MAX(0, srtmove - train->transition.shortmove);
            train->microPerTick = (srtmove * 1000) / timeTransition;
            train->transition.shortmove = srtmove + traveledDist;
        }
        /* detect if we've stopped our transition acceleration/deceleration state */
        if (timeTransition >= getTransitionTicks(train->id, start_speed, stop_speed)) {
            debug("Train %u: Transition finished at time %u", train->id, ticks);
            train->transition.valid = false;
            train->microPerTick = getTrainVelocity(train->id, stop_speed);
            train->speed = stop_speed;
            traveledDist = train->transition.stopping_distance;
            if (stop_speed == 0) {
                fullStop = true;
            }
        }
    } else {
        traveledDist += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    train->distSinceLastSensor += traveledDist;
    train->distSinceLastNode += traveledDist;
    train->pathRemain -= traveledDist;
    train->resv.extraResvDist -= traveledDist;
    train->lastUpdateTick = ticks;

    if (traveledDist != 0) {
        distTraverse(train, false);
    }

    if (fullStop == true) {
        freeResvOnStop(train);
    }

    // out of resv distance and still moving
    if (train->resv.extraResvDist <= 0
         && (train->speed != 0 || (train->transition.valid && train->transition.dest_speed != 0))) {
        track_node *lastResv = peek_back_Resv(&(train->resv));
        int stoppingDistResv = RESV_DIST(train->stoppingDist - train->currentEdge->dist + train->distSinceLastNode);
        int distToResv = MAX(train->distToNextSensor - train->distSinceLastSensor, stoppingDistResv);

        Log("UpdateLocation: at %d after %s\n", train->distSinceLastNode, train->currentEdge->src->name);
        Log("UpdateLocation: Resv: sensor dist {%d}, stopping dist {%d}\n",
            train->distToNextSensor - train->distSinceLastSensor, stoppingDistResv);

        train->resv.extraResvDist = -reserveTrack(train, distToResv);

        Log("UpdateLocation: Extra resv dist after resv: %d\n", train->resv.extraResvDist);

        if (train->resv.extraResvDist < 0) {
            /* a collision has occured */
            Log("updateLocation - Collision detected on train %d, stopping\n", train->id);
            setTrainSpeed(train, 0);
            train->gotoResult = GOTO_REROUTE;
        }

        if (lastResv != peek_back_Resv(&(train->resv))) {
            execPath(train, lastResv);
        }
    }

    if (train->path && train->pathRemain < train->stoppingDist + STOP_BUFFER_MM) {
        train->path = NULL;
        Log("Train %u: Critical path remaining reached at %d for stopping dist %d, train loc %d after %s, stopping\n",
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
        Log("waiting on %s", train->nextSensor->name);
        *waitingSensor = train->nextSensor->num;
        *sensorCourier = Create(4, SensorCourierTask);
        msg1.type = TRM_SENSOR_WAIT;
        msg1.arg0 = train->nextSensor->num;
        Send(*sensorCourier, &msg1, sizeof(msg1), NULL, 0);

        if (train->speed != 0 && !train->transition.valid) {
            int timeout = 20 + (train->distToNextSensor * 1000) / train->microPerTick;
            *timeoutCourier = CourierDelay(timeout, 4);
            Log(" with timeout %d with tid %d\n", timeout, *timeoutCourier);
        }
    }
}


static void setTrainSpeed(Train_t *train, int speed) {
    char buf[2];
    Log("setTrainSpeed: Train %u before updateNextServer in setTrainSpeed\r\n", train->id);
    updateNextSensor(train);
    Log("setTrainSpeed: Train %u after updateNextServer in setTrainSpeed\r\n", train->id);
    if (speed != train->speed) {
        int tick = Time();
        train->stoppingDist = getStoppingDistance(train->id, train->speed, speed);
        train->transition.valid = true;
        train->transition.start_speed = train->speed;
        train->transition.dest_speed = speed;
        train->transition.time_issued = tick;
        train->transition.shortmove = 0;
        train->lastUpdateTick = tick;
        train->lastSensorTick = -1;
        // debug("Stopping Distance for %d -> %d: %d", train->speed, speed, train->stoppingDist);
        if (speed == 0) {
            train->transition.stopping_distance = train->stoppingDist;
            Log("=====Train speed set to 0 at tick %d, currently %d past %s with stopping_distance = %d=====\n",
                tick, train->distSinceLastNode, train->currentEdge->src->name, train->transition.stopping_distance);
        } else {
            train->transition.stopping_distance = 0;
        }
        train->speed = speed;
    }
    buf[0] = train->speed + train->aux;
    buf[1] = train->id;
    trnputs(buf, 2);
}

static int trainDir(Train_t *train) {
    if (train->speed != 0 || train->transition.valid) {
        error("Train %u: Cannot reverse while moving, ticks: %u, called by: %u",
                train->id, Time());
        return -1;
    }

    ASSERT(train->currentEdge, "Current edge is NULL pre-rev\n");

    Log("Rev: initial %d from %s\n", train->distSinceLastNode, train->currentEdge->src->name);

    /* recompute distances from sensors and nodes */
    train->currentEdge = train->currentEdge->reverse;
    train->distSinceLastNode = train->currentEdge->dist - train->distSinceLastNode;

    ASSERT(train->currentEdge, "Current edge is NULL post-rev\n");

    Log("Rev: now %d from %s\n", train->distSinceLastNode, train->currentEdge->src->name);

    updateNextSensor(train);

    Log("Updated sensor to %s\n", (train->nextSensor ? train->nextSensor->name : "NULL"));

    if (DispatchReserveTrack(train->id, &(train->currentEdge->src), 1) == 1) {
        freeHeadResv(train);
        push_back_Resv(&(train->resv), train->currentEdge->src);
    } else {
        /* TODO: Handle this in caller */
        error("Train %u: Reverse reservation failed for %s", train->id, train->currentEdge->src->name);
        return -2;
    }

    Log("rev success\n");

    char command[2];
    command[0] = TRAIN_AUX_REVERSE;
    command[1] = train->id;
    trnputs(command, 2);

    if (train->pickupOffset == PICKUP_FRONT) {
        train->pickupOffset = PICKUP_BACK;
    } else if (train->pickupOffset == PICKUP_BACK) {
        train->pickupOffset = PICKUP_FRONT;
    } else {
        Panic("Pickup offset is %d, which is neither PICKUP_FRONT (%d) or PICKUP_BACK (%d) WHAT THE FUCK DID YOU DO?",
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
    train->transition.valid = false;
    train->transition.start_speed = 0;
    train->transition.dest_speed = 0;
    train->transition.time_issued = 0;
    train->transition.stopping_distance = 0;
    train->transition.shortmove = 0;

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

    ASSERT(train->currentEdge != NULL, "Train current edge is NULL WAT\n");

    updateNextSensor(train);

    // reserve your initial sensor
    int numSuccessResv = DispatchReserveTrack(train->id, &(train->currentEdge->src), 1);
    ASSERT(numSuccessResv, "Train initial sensor reservation failed");
    push_back_Resv(&(train->resv), train->currentEdge->src);
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
        /* adjust by five(5) to compensate for the clock edge */
        Delay(getTransitionTicks(request.tr, request.arg0, 0) + STOP_BUFFER_MM);
        debug("ReverseCourier: Reversing train tid %u, called at time %u", callee, Time());
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

        status = 0;
        if (callee == sensorCourier || callee == timeoutCourier) {
            if (callee == timeoutCourier) {
                Log("Train %u: Timeout on sensor %s\r\n", train.id,
                    (train.nextSensor != NULL ? d(train.nextSensor).name : "NULL"));
                if (train.nextSensor != NULL && ++numSensorMissed > SENSOR_MISS_THRESHOLD) {
                    Log("<<<<Too many timeouts, stopping>>>>\n");
                    setTrainSpeed(&train, 0);
                    train.gotoResult = GOTO_LOST;
                    continue;
                }
            } else {
                numSensorMissed = 0;
            }

            if (train.nextSensor != NULL) {
                Log("Before Sensor trip - Next sensor: %s\n", d(train.nextSensor).name);
                sensorTrip(&train, train.nextSensor);
                if (train.nextSensor) {
                    Log("Sensor trip - Next sensor: %s\n", d(train.nextSensor).name);
                    waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                    train.gotoResult = GOTO_COMPLETE;
                } else {
                    Log("Sensor trip - next sensor NULL, should reverse on stop\n");
                }
            } else {
                ASSERT(false, "This should never actually happen WAT\n");
            }
            CalibrationSnapshot(&train);
            Log("Sensor trip - done\n\n");
            continue;
        } else if (callee == shortMvStop) {
            shortMvStop = -1;
            Log("===shortMvStop at TICK {%d}===\n", train.lastUpdateTick);
            trainSpeed(train.id, 0);
            Reply(callee, NULL, 0);
            continue;
        } else if (callee == shortMvDone) {
            shortMvDone = -1;
            Reply(callee, NULL, 0);
            // set this to true so next updateLocation call will push us to the final location
            Log("===shortMvDone at %d===\nBEFORE: at %d after %s\n", train.lastUpdateTick,
                train.distSinceLastNode, train.currentEdge->src->name);
            train.distSinceLastNode += train.transition.stopping_distance;
            distTraverse(&train, false);
            if (train.distSinceLastNode >= train.currentEdge->dist) {
                train.gotoResult = GOTO_LOST;
            } else {
                freeResvOnStop(&train);
                train.gotoResult = GOTO_COMPLETE;
            }
            Log("AFTER: at %d after %s, edge dist %d\n", train.distSinceLastNode,
                train.currentEdge->src->name, train.currentEdge->dist);
            continue;
        }

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
                while (peek_head_Resv(&(train.resv)) != NULL) {
                    freeHeadResv(&train);
                }

                Destroy(locationTimer);
                status++;
                Reply(callee, &status, sizeof(status));
                notice("Train %u: Deleted train %u  (Tid %u)", train.id, train.id, MyTid());
                clearTrainSnapshot(train.id);
                Exit();
                break;
            case TRM_GOTO_AFTER:
                /* blocks the calling task until we arrive at our destination */
                gotoBlocked = callee;
                debug("Train %u: Received request from Conductor (Tid %u) at %d after %s",
                      train.id, gotoBlocked, train.distSinceLastNode, train.currentEdge->src->name);
                callee = -1;
                if (request.arg0 == NULL) {
                    train.pathNodeRem = 0;
                    train.pathRemain = request.arg2;
                } else {
                    train.path = (track_node**)request.arg0;
                    train.pathNodeRem = request.arg1;
                    train.distOffset = request.arg2;
                    train.pathRemain = pathRemaining(&train);
                    Log(">>>>>>>>>>>Exec move setup for train %u with dist %d\n", train.id, train.pathRemain);
                    updateNextSensor(&train);

                    train.resv.extraResvDist = -reserveTrack(&train, MAX(train.distToNextSensor,
                        RESV_DIST(train.stoppingDist - train.currentEdge->dist)));
                    if (train.resv.extraResvDist >= 0) {
                        /* remove the previously reserved nodes */
                        while (peek_head_Resv(&(train.resv)) != train.currentEdge->dest) {
                            freeHeadResv(&train);
                        }
                        execPath(&train, NULL);
                    } else {
                        // resv failed, undo
                        while (size_Resv(&(train.resv)) > 1) {
                            freeTailResv(&train);
                        }
                        train.gotoResult = GOTO_REROUTE;
                        break;
                    }

                    Log("<<<<<<<<<<<Finish Exec move setup for train %u \n", train.id);
                }

                // 2x stopping dist using short moves will probably break shit
                // TODO: use piece-wise function
                int stoppingDist = getStoppingDistance(train.id, GOTO_SPEED, 0);
                if (train.pathRemain < stoppingDist + (stoppingDist >> 1)) {
                    Log(">>>>>>>>>>Exec SHORT move at %d after %s\n", train.distSinceLastNode, train.currentEdge->src->name);
                    int delayTime;
                    if (train.pathRemain < stoppingDist) {
                        delayTime = shortmoves(train.id, GOTO_SPEED, train.pathRemain);
                    } else {
                        delayTime = shortmoves(train.id, GOTO_SPEED, stoppingDist) +
                            ((train.pathRemain - stoppingDist) * 500 / getTrainVelocity(train.id, GOTO_SPEED));
                    }
                    Log("%d : Train %u: Short move with delay %d ticks and %d path nodes to cover %d mm\n",
                           Time(), train.id, delayTime, train.pathNodeRem, train.pathRemain);

                    trainSpeed(train.id, GOTO_SPEED);
                    train.transition.stopping_distance = train.pathRemain;
                    shortMvStop = CourierDelay(delayTime, 4);
                    shortMvDone = CourierDelay(delayTime << 1, 4);
                    train.path = NULL;
                } else {
                    Log(">>>>>>>>>>Exec NORMAL move at %d after %s\n", train.distSinceLastNode, train.currentEdge->src->name);
                    // normal move
                    setTrainSpeed(&train, GOTO_SPEED);
                }
                waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                Log("=================TRM_GOTO DONE\n");
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

                if (speed == 0) {
                    setTrainSpeed(&train, speed);
                } else {
                    train.resv.extraResvDist = -reserveTrack(&train, MAX(train.distToNextSensor,
                        RESV_DIST(train.stoppingDist - train.currentEdge->dist)));
                    if (train.resv.extraResvDist >= 0) {
                        /* remove the previously reserved nodes */
                        while (peek_head_Resv(&(train.resv)) != train.currentEdge->dest) {
                            freeHeadResv(&train);
                        }
                        dump_Resv(&(train.resv));
                        setTrainSpeed(&train, speed);
                        waitOnNextTarget(&train, &sensorCourier, &waitingSensor, &timeoutCourier);
                    } else {
                        // resv failed, undo
                        while (size_Resv(&(train.resv)) > 1) {
                            freeTailResv(&train);
                        }
                    }
                }
                break;
            case TRM_AUX:
                if (request.arg0 == TRAIN_LIGHT_OFFSET && train.aux == TRAIN_LIGHT_OFFSET) {
                    train.aux = 0;
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
                if (gotoBlocked != -1 && train.gotoResult == GOTO_LOST) {
                    // totally lost? ask for help!
                    Log("Train %u: Lost!!!!!! shit shit shit\n", train.id);
                    Reply(gotoBlocked, &(train.gotoResult), sizeof(train.gotoResult));
                    gotoBlocked = -1;
                }

                if (train.speed == 0 && gotoBlocked != -1 && (train.transition.valid == false) && shortMvDone == -1) {
                    if (train.distSinceLastNode > train.currentEdge->dist) {
                        train.gotoResult = GOTO_LOST;
                    }
                    Log("Train %u: Finished pathing, replying to conductor (%d) with result %d\n",
                        train.id, gotoBlocked, train.gotoResult);
                    Reply(gotoBlocked, &(train.gotoResult), sizeof(train.gotoResult));
                    gotoBlocked = -1;
                }

                request.arg0 = (int)train.currentEdge->src;
                request.arg1 = train.distSinceLastNode;
                Reply(callee, &request, sizeof(request));
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
