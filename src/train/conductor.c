#include <conductor.h>
#include <syscall.h>
#include <train.h>
#include <dispatcher.h>
#include <track_node.h>
#include <train_task.h>
#include <train_speed.h>
#include <term.h>
#include <path.h>

typedef struct {
    int type;
    int arg0;
    int arg1;
    int arg2;
} ConductorMessage_t;

typedef enum {
    GOTO = 33,
    TIME_UP,
    DIST_UP
} ConductorMessageTypes;


static int getOptimalSpeed(unsigned int tr, unsigned int distance) {
    unsigned int i;
    for (i = TRAIN_MAX_SPEED - 2; i > 1; ++i) {
        if (getStoppingDistance(tr, i, 0) <= distance) {
            break;
        }
    }
    return i;
}


void Conductor() {
    int callee, status;
    unsigned int train;
    TrainMessage_t msg;
    ConductorMessage_t req;
    track_node *source, *dest, *path[32] = {0};
    unsigned int node_count, i;
    unsigned int total_distance, edgeDist, destDist;

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
    source = TrGetLocation(train, &edgeDist);
    dest = (track_node*)req.arg1;
    destDist = req.arg2;
    if ((node_count = findPath(source, dest, path, 32, NULL, 0, &total_distance)) < 0) {
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
        msg.type = TRM_GOTO;
        msg.arg0 = total_distance + destDist;
        Send(train, &msg, sizeof(msg), &status, sizeof(status));
        TrSpeed(train, getOptimalSpeed(status, total_distance));
    }
    /* TODO: Block on child's destination or have another one block for you */
    req.type = TRM_GOTO_STOP;
    req.arg0 = train;
    /* parent actually destroys this task here */
    Send(MyParentTid(), &req, sizeof(req), &status, sizeof(status));
    Exit();
}


int GoTo(unsigned int tid, unsigned int tr, track_node *sensor, unsigned int distance) {
    ConductorMessage_t req;
    int status;

    req.type = GOTO;
    req.arg0 = tr;
    req.arg1 = (int)sensor;
    req.arg2 = distance;
    Send(tid, &req, sizeof(req), &status, sizeof(status));
    return status;
}
