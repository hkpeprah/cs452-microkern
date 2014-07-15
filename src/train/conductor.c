#include <conductor.h>
#include <syscall.h>
#include <train.h>
#include <dispatcher.h>
#include <track_node.h>
#include <train_task.h>
#include <train_speed.h>
#include <term.h>
#include <path.h>
#include <stdlib.h>

typedef struct {
    int type;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
} ConductorMessage_t;

typedef enum {
    GOTO = 33,
    MOVE,
    TIME_UP,
    DIST_UP
} ConductorMessageTypes;


/*
static int getOptimalSpeed(unsigned int tr, unsigned int distance) {
    unsigned int i;
    for (i = TRAIN_MAX_SPEED - 2; i > 1; ++i) {
        if (getStoppingDistance(tr, i, 0) <= distance) {
            break;
        }
    }
    return i;
}
*/


void Conductor() {
    int callee, status;
    unsigned int train;
    ConductorMessage_t req;
    track_node *source, *dest, *path[32] = {0};
    unsigned int node_count, i;
    unsigned int total_distance, nextSensorDist, destDist;

    source = NULL;
    status = Receive(&callee, &req, sizeof(req));
    if (status < 0) {
        error("Conductor: Error: Error in send, received %d from %d", status, callee);
        Reply(callee, NULL, 0);
        Exit();
    }
    status = 1;
    Reply(callee, &status, sizeof(status));

    train = req.arg0;
    source = TrGetNextLocation(train, &nextSensorDist);
    ASSERT((req.type == GOTO || req.type == MOVE), "conductor recieved a message not of type GOTO or MOVE");

    /* check if goto or just a short move */
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;
        if ((node_count = findPath(req.arg3, source, dest, path, 32, &total_distance)) < 0) {
            error("Conductor: Error: No path to destination %u found", dest->num);
            Exit();
        } else {
            /* set up the couriers to navigate to destination */
            for (i = 0; i < node_count; ++i) {
                #if DEBUG
                printf((i == node_count - 1 ? "%s(%d)\r\n" : "%s(%d) -> "), path[i]->name, path[i]->num);
                #endif
                if (path[i]->type == NODE_BRANCH) {
                    if (path[i]->edge[DIR_STRAIGHT].dest == path[i + 1]) {
                        trainSwitch(path[i]->num, 'S');
                    } else if (path[i]->edge[DIR_CURVED].dest == path[i + 1]) {
                        trainSwitch(path[i]->num, 'C');
                    }
                }
            }
        }
        TrGotoAfter(train, path, node_count, destDist);
    } else {
        destDist = req.arg2;
        TrGotoAfter(train, NULL, 0, destDist);
    }

    status = DispatchStopRoute(req.arg3);
    if (status < 0) {
        error("Conductor: Error: Got %d in send to parent %u", status, MyParentTid());
    }
    debug("Conductor: Tid %u, removing self from parent %u", MyTid(), MyParentTid());
    Exit();
}


int GoTo(unsigned int tid, unsigned int train, unsigned int tr_number, track_node *sensor, unsigned int distance) {
    ConductorMessage_t req;
    int status;

    req.type = GOTO;
    req.arg0 = train;
    req.arg1 = (int)sensor;
    req.arg2 = distance;
    req.arg3 = tr_number;
    Send(tid, &req, sizeof(req), &status, sizeof(status));
    return status;
}


int Move(unsigned int tid, unsigned int train, unsigned int tr_number, unsigned int distance) {
    ConductorMessage_t req;
    int status;

    req.type = MOVE;
    req.arg0 = train;
    req.arg1 = 0;
    req.arg2 = distance;
    req.arg3 = tr_number;
    Send(tid, &req, sizeof(req), &status, sizeof(status));
    return status;
}
