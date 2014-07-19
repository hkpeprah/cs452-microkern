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
    track_node *dest, *path[32] = {0};
    track_edge *source;
    unsigned int node_count, fractured;
    unsigned int total_distance, destDist;

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
    source = TrGetEdge(train);
    ASSERT((req.type == GOTO || req.type == MOVE), "conductor recieved a message not of type GOTO or MOVE");
    /* check if goto or just a short move */
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;
        if ((node_count = findPath(req.arg3, source, dest, path, 32, &total_distance)) < 0) {
            error("Conductor: Error: No path to destination %u found", dest->num);
            Exit();
        }

        if (path[0] != source->dest) {
            // TODO: only this when stopped
            TrDirection(train);
        }

        int base = 0, i;

        for (i = 0; i < node_count - 1; ++i) {
            if (path[i]->reverse == path[i + 1]) {
                TrGotoAfter(train, &(path[base]), (i - base + 1), 100);
                TrDirection(train);
                base = ++i;
            }
        }

        if (base != node_count) {
            TrGotoAfter(train, &(path[base]), node_count - base, 0);
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
