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
#include <clock.h>

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


void Conductor() {
    int callee, status;
    unsigned int train;
    ConductorMessage_t req;
    track_node *source, *dest, *path[32] = {0};
    unsigned int node_count, i, fractured;
    unsigned int total_distance, nextSensorDist, destDist;

    status = Receive(&callee, &req, sizeof(req));
    if (status < 0) {
        error("Conductor: Error: Error in send, received %d from %d", status, callee);
        Reply(callee, NULL, 0);
        Exit();
    }
    status = 1;
    Reply(callee, &status, sizeof(status));

    fractured = 0;
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
        }

        /* break up path into forward and back tracks */
        for (i = 0; i < node_count; ++i) {
            if (path[i]->reverse == path[i + 1]) {
                /* if next path is a reveral, reverse */
                if (fractured > 0) {
                    /* check if we had path leading up to the reverse, if so, goto first */
                    printf("%s(%d)@[%d]\r\n", path[i]->name, path[i]->num, i);
                    if (i == node_count - 2) {
                        printf("Finished pathing.\r\n");
                    }
                    Delay(20);
                    TrGotoAfter(train, &(path[i - fractured]), fractured + 1, 0);
                    fractured = 0;
                }
                printf("%s(%d)@[%d] <-> ", path[i]->name, path[i]->num, i);
                Delay(20);
                TrDirection(train);
            } else if (i == node_count - 1) {
                /* if we have fractals and we have exhausted our nodes, just move */
                printf("%s(%d)@[%d]\r\nFinished pathing.\r\n", path[i]->name, path[i]->num, i);
                Delay(20);
                TrGotoAfter(train, &(path[i - fractured]), fractured + 1, destDist);
            } else {
                printf("%s(%d)@[%d] -> ", path[i]->name, path[i]->num, i);
                fractured++;
            }
        }
    } else {
        /* TODO: Traverse to find path */
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
