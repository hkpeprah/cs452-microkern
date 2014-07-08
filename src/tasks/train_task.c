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
    track_node *nextSensor;
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
    TRM_GOTO,
    TRM_AUX,
    TRM_RV,
    TRM_GET_LOCATION,
    TRM_GET_SPEED,
    TRM_GET_PATH,
    TRM_DIR,
    TRM_BLK_LNDMARK,
    TRM_RT_UNBLK
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


int TrGoTo(unsigned int tid, track_node *finalDestination) {
    TrainMessage_t msg = {.type = TRM_GOTO, .arg0 = (int)finalDestination};
    int status;

    if (finalDestination != NULL) {
        status = 0;
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
    if (callee != train) {
        error("WatchDog: Got message from something not the train.");
        Reply(callee, &request, sizeof(request));
        return;
    }

    wait = request.arg0;
    status = Reply(callee, &request, sizeof(request));
    if (request.type != TRM_TIME_WAIT) {
        error("WatchDog: Bad message type %d from %d", request.type, train);
        return;
    }

    //debug("WatchDog: Delaying for %u ticks", wait);
    if (Delay(wait + TIMEOUT_BUFFER) < 0) {
        error("WatchDog: Something went wrong with delay.");
    }
    //debug("WatchDog awoken after %u ticks", wait);
    request.type = TRM_TIME_WAIT;
    Send(train, &request, sizeof(request), &status, sizeof(status));
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

    //debug("Sensor Courier Task: waiting on sensor %d", wait);
    if ( (status = WaitOnSensorN(wait)) < 0) {
        error("SensorCourier: result from WaitOnSensorN: %d", status);
    }
    //debug("sensor %d trip", wait);
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
        // Delay(getTransitionTicks(request.arg1, 0, request.arg0));
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
        train->edgeDistance = 0;
    }
    train->currentEdge = getNextEdge(dest);
}


static void updateLocation(Train_t *train) {
    unsigned int ticks, transition_ticks;

    ticks = Time();
    /* account for acceleration/deceleration by considering
     * the transition state */
    if (train->transition->valid) {
        transition_ticks = getTransitionTicks(train->id, train->transition->start_speed, train->transition->dest_speed);
        if (transition_ticks <= (ticks - train->transition->time_issued)) {
            /* we've passed our transition period */
            train->transition->valid = false;
            train->edgeDistance += (ticks - transition_ticks - train->transition->time_issued) * train->microPerTick / 1000;
            train->edgeDistance += getStoppingDistance(train->id, train->transition->start_speed, train->transition->dest_speed);
        } else {
            /* otherwise we're still in it */
            transition_ticks = ticks - train->transition->time_issued;
            train->edgeDistance += getTransitionDistance(train->id, train->transition->start_speed, train->transition->dest_speed, transition_ticks);
        }

    } else {
        train->edgeDistance += (ticks - train->lastUpdateTick) * train->microPerTick / 1000;
    }

    train->lastUpdateTick = ticks;
    CalibrationSnapshot(train);
}


static int WaitOnNextTarget(Train_t *train, int *SensorCourier, int *WatchDog, int *waitingSensor) {
    TrainMessage_t msg1, msg2;
    track_node *dest;
    track_edge *nextEdge;
    int start_speed, dest_speed;
    int distance;
    unsigned int ticks, timeout, velocity, currentTime;

    if (*waitingSensor >= 0) {
        FreeSensor(*waitingSensor);
    }

    if (*WatchDog >= 0) {
        Destroy(*WatchDog);
        *WatchDog = -1;
    }

    if (*SensorCourier >= 0) {
        Destroy(*SensorCourier);
        *SensorCourier = -1;
    }

    if (train->speed == 0 && !train->transition->valid) {
        train->nextSensor = NULL;
        return 0;
    }

    currentTime = train->lastUpdateTick;
    dest = train->currentEdge->dest;

    if (train->transition->valid) {
        start_speed = train->transition->start_speed;
        dest_speed = train->transition->dest_speed;
        /* compute distance travelled accelerating/decelerating, and the number
         * of ticks it takes to make that. */
        distance = getStoppingDistance(train->id, start_speed, dest_speed) * 1000;
        ticks = getTransitionTicks(train->id, start_speed, dest_speed);
        if (ticks <= (currentTime - train->transition->time_issued)) {
            train->transition->valid = false;
        }
        /* fix distance to be from the start speed to destination speed */
        velocity = distance / ticks;
    } else {
        velocity = train->microPerTick;
    }

    distance = train->currentEdge->dist - train->edgeDistance;
    while (dest && dest->type != NODE_SENSOR) {
        nextEdge = getNextEdge(dest);
        dest = nextEdge->dest;
        distance += nextEdge->dist;
    }

    train->nextSensor = dest;
    *waitingSensor = dest->num;
    *SensorCourier = Create(4, SensorCourierTask);
    msg1.type = TRM_SENSOR_WAIT;
    msg1.arg0 = dest->num;
    Send(*SensorCourier, &msg1, sizeof(msg1), NULL, 0);

    timeout = (distance * 1000) / velocity;
    if (timeout > 0 && (train->speed > 0 || train->transition->valid)) {
        *WatchDog = Create(3, TrainWatchDog);
        msg2.type = TRM_TIME_WAIT;
        msg2.arg0 = timeout;
        Send(*WatchDog, &msg2, sizeof(msg2), &msg2, sizeof(msg2));
    }

    return timeout;
}

static void trainSpeed(Train_t *train, int speed) {
    char command[2];
    if (speed != train->speed) {
        /* compensate for acceleration */
        train->transition->valid = true;
        train->transition->start_speed = train->speed;
        train->transition->dest_speed = speed;
        train->transition->time_issued = Time();
        train->speed = speed;
        train->microPerTick = getTrainVelocity(train->id, train->speed);
        command[0] = train->speed + train->aux;
        command[1] = train->id;
        trnputs(command, 2);
        updateLocation(train);
    }
}

static void updatePath(track_node *src, track_node *dest) {
    track_node *p[64] = {0};
    track_node **path = p;
    unsigned int len;
    findPath(src, dest, path, 64, NULL, 0, &len);

    track_node *currentNode;
    track_node *nextNode;

    while (*path) {
        currentNode = *(path++);
        nextNode = *path;
        track_edge *edge = currentNode->edge;

        if (currentNode == nextNode->reverse) {
            // reverse, not implemented
        } else if (currentNode->type == NODE_BRANCH) {
            // branching switch
            if (edge[DIR_CURVED].dest == nextNode) {
                trainSwitch(currentNode->num, 'C');
                // special tri-switches, switch their pair
                if (currentNode->num == 154 || currentNode->num == 156) {
                    trainSwitch(currentNode->num - 2, 'C');
                }
            } else if (edge[DIR_STRAIGHT].dest == nextNode) {
                trainSwitch(currentNode->num, 'S');
            }
        }
    }
}


static void TrainEngineer() {
    track_node *path[32] = {0};
    TrainMessage_t request;
    track_node *source, *destination, *node;
    unsigned int start_speed, total_distance, tr, i, ticks, stopping_time, velocity;
    int node_count, status, callee, train, current_path_index;

    train = MyParentTid();
    status = Receive(&callee, &request, sizeof(request));
    if (callee != train) {
        Reply(callee, &status, sizeof(status));
        error("TrainEngineer: Error: Received message from Task with ID %u not parent", callee);
        Exit();
    } else if (request.type != TRM_GET_PATH) {
        Reply(callee, &status, sizeof(status));
        error("TrainEngineer: Error: Received message not of type get path");
        Exit();
    }

    current_path_index = 0;
    source = (track_node*)request.arg0;
    destination = (track_node*)request.arg1;
    tr = request.arg2;

    /* TODO: Incorporate unavailable edges */
    node_count = findPath(source, destination, path, 32, NULL, 0, &total_distance);
    if (node_count <= 0) {
        status = -1;
        error("TrainEngineer: Error: findPath returned a node count value of %d", node_count);
        Reply(callee, &status, sizeof(status));
        Exit();
    }

    Reply(callee, &status, sizeof(status));
    debug("Node Count: %d, Source: %u, Dest: %x", node_count, node_count, source, destination);

    #if DEBUG
        for (i = 0; i < node_count; ++i) {
            if (i == node_count - 1) {
                printf("%s(%d)", path[i]->name, path[i]->num);
            } else {
                printf("%s(%d) -> ", path[i]->name, path[i]->num);
            }
        }
        printf("\r\n");
    #endif

    for (i = TRAIN_MAX_SPEED; i > 0; --i) {
        if (getStoppingDistance(tr, i, 0) <= total_distance) {
            break;
        }
    }

    /* reduce ticks to the time it will take to stop */
    start_speed = i;
    stopping_time = getTransitionTicks(tr, start_speed, 0);
    velocity = getTrainVelocity(tr, start_speed);
    ticks = Time() + (total_distance / velocity);
    ticks -= getTransitionTicks(tr, start_speed, 0);
    request.type = TRM_BLK_LNDMARK;
    TrSpeed(train, start_speed);

    debug("TrainEngineer: Trip should tick at most %u ticks", ticks);
    while ((node = path[current_path_index]) != destination && ticks <= Time() && node_count > 0) {
        current_path_index++;
        node_count--;
        if (path[current_path_index] == node->reverse) {
            debug("TrainEngineer: Reversing the train.");
            TrReverse(train);
            Delay(stopping_time + 40);
            if (ABS(node->num - path[current_path_index]->num) == 1) {
                /* special case where we wait on ourselves */
                debug("TrainEngineer: Node actually self reversed.");
                current_path_index++;
                node_count--;
            }
            continue;
        } else if (path[current_path_index]->type != NODE_SENSOR) {
            if (path[current_path_index]->type == NODE_MERGE) {
                if (path[current_path_index]->reverse == path[current_path_index + 1]) {
                    Delay(MAX(path[current_path_index]->edge[DIR_STRAIGHT].dist,
                              path[current_path_index]->edge[DIR_CURVED].dist) * 1000 / velocity);
                    TrReverse(train);
                    Delay(stopping_time + 40);
                    current_path_index++;
                    node_count--;
                }
                continue;
            } else if (path[current_path_index]->type == NODE_BRANCH) {
                if (path[current_path_index]->edge[DIR_STRAIGHT].dest == path[current_path_index + 1]) {
                    debug("TrainEngineer: Toggling switch %u to straight", path[current_path_index]->num);
                    trainSwitch(path[current_path_index]->num, 'S');
                } else if (path[current_path_index]->edge[DIR_CURVED].dest == path[current_path_index + 1]) {
                    debug("TrainEngineer: Toggling switch %u to curved", path[current_path_index]->num);
                    trainSwitch(path[current_path_index]->num, 'C');
                }
                continue;
            }
        }
        request.arg0 = path[current_path_index]->num;
        debug("TrainEngineer: Moving towards: %d", request.arg0);
        Send(train, &request, sizeof(request), &status, sizeof(status));
    }

    TrSpeed(train, 0);
    request.type = TRM_RT_UNBLK;
    Send(train, &request, sizeof(request), &status, sizeof(status));
    Exit();
}


static void TrainTimer() {
    TrainMessage_t msg = {.type = TRM_GET_LOCATION};
    int parent = MyParentTid();

    while (true) {
        Delay(50);
        Send(parent, &msg, sizeof(msg), NULL, 0);
    }

    Exit();
}


static void TrainTask() {
    TrainMessage_t request, message;
    Train_t train = {0};
    track_node *dest = NULL;
    char command[2], name[] = "TrainXX";
    int status, bytes, callee;
    short speed;
    TransitionState_t state;
    int waitingSensor, engineerWaiting;
    int expectSensorTripTime = 0;
    int SensorCourier, ReverseCourier, WatchDog, Engineer, timer;

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

    state.valid = false;

    name[5] = (train.id / 10) + '0';
    name[6] = (train.id % 10) + '0';
    ReverseCourier = 0;
    waitingSensor = -1;
    WatchDog = -1;
    SensorCourier = -1;
    Engineer = 0;
    engineerWaiting = -1;

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
        if (callee == SensorCourier || callee == WatchDog) {
            if (train.nextSensor) {
                traverseNode(&train, train.nextSensor);
            }

            debug("trip - expect {%d} actual {%d}\n", expectSensorTripTime, train.lastUpdateTick);
            CalibrationSnapshot(&train);
            if (train.currentEdge->src->num == engineerWaiting) {
                Reply(Engineer, &status, sizeof(status));
                engineerWaiting = -1;
            }
            expectSensorTripTime = train.lastUpdateTick + WaitOnNextTarget(&train, &SensorCourier, &WatchDog, &waitingSensor);

            if (train.nextSensor == dest) {
                trainSpeed(&train, 0);
                dest = NULL;
            }
            continue;
        }

        if (engineerWaiting >= 0 && !(callee == Engineer || callee == ReverseCourier)) {
            status = -2;
            Reply(callee, &status, sizeof(status));
            continue;
        }

        switch (request.type) {
            case TRM_TIME_READY:
                break;
            case TRM_BLK_LNDMARK:
                if (callee == Engineer) {
                    engineerWaiting = request.arg0;
                } else {
                    status = -1;
                    error("Train %u: Something not Engineer tried to block", train.id);
                    Reply(callee, &status, sizeof(status));
                }
                break;
            case TRM_SPEED:
                speed = request.arg0;

                /* don't care if the speeds are the same */
                if (speed != train.speed) {
                    trainSpeed(&train, speed);
                    expectSensorTripTime = train.lastUpdateTick + WaitOnNextTarget(&train, &SensorCourier, &WatchDog, &waitingSensor);
                }

                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case TRM_RT_UNBLK:
                Engineer = 0;
                break;
            case TRM_GOTO:
            /*
                Engineer = Create(6, TrainEngineer);
                message.type = TRM_GET_PATH;
                message.arg0 = (int)train.currentEdge->src;
                message.arg1 = request.arg0;
                message.arg2 = train.id;
                Send(Engineer, &message, sizeof(message), &status, sizeof(status));
            */
                dest = (track_node*) request.arg0;
                updatePath(train.currentEdge->dest, dest);
                trainSpeed(&train, 10);
                expectSensorTripTime = train.lastUpdateTick + WaitOnNextTarget(&train, &SensorCourier, &WatchDog, &waitingSensor);

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
