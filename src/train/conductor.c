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
    ConductorMessage_t req;
    track_edge *source;
    track_node *dest, *path[32] = {0};
    int callee, status, myTid, train;
    unsigned int node_count;
    unsigned int total_distance, destDist;

    status = Receive(&callee, &req, sizeof(req));
    if (status < 0) {
        error("Conductor: Error: Error in send, received %d from %d", status, callee);
        Reply(callee, NULL, 0);
        Exit();
    }
    status = 1;
    Reply(callee, &status, sizeof(status));

    train = req.arg0;
    source = TrGetEdge(train);
    myTid = MyTid();

    ASSERT((req.type == GOTO || req.type == MOVE),
           "conductor recieved a message not of type GOTO or MOVE: type %d from %d", req.type, callee);

    /* check if goto or just a short move */
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;
        if ((node_count = findPath(req.arg3, source, dest, path, 32, &total_distance)) < 0) {
            error("Conductor: Error: No path to destination %u found", dest->num);
            DispatchStopRoute(req.arg3);
            Exit();
        }

        if (path == NULL) {
            DispatchStopRoute(req.arg3);
            Exit();
        }

        if (path[0] != source->dest) {
            // TODO: only this when stopped
            TrDirection(train);
        }

        int i, base = 0;
        for (i = 0; i < node_count - 1; ++i) {
            if (path[i]->reverse == path[i + 1]) {
                TrGotoAfter(train, &(path[base]), (i - base + 1), 150);
                if ((status == TrDirection(train)) < 0) {
                    error("Conductor[%u]: Train %u[%u] cannot reverse, %s, dying...", myTid, req.arg3,
                          train, (status == -2 ? "could not reserve reverse" : "train is moving"));
                    DispatchStopRoute(req.arg3);
                    Exit();
                }
                base = ++i;
            }
        }

        if (base != node_count) {
            TrGotoAfter(train, &(path[base]), node_count - base, 0);
        }
    } else {
        destDist = req.arg2;
        TrGotoAfter(train, NULL, 0, destDist);
    }

    status = DispatchStopRoute(req.arg3);
    if (status < 0) {
        error("Conductor: Error: Got %d in send to parent %u", status, MyParentTid());
    }
    debug("Conductor: Tid %u, removing self from parent %u", myTid, MyParentTid());
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
