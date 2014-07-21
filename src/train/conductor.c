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
#include <random.h>
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
    myTid = MyTid();

    ASSERT((req.type == GOTO || req.type == MOVE),
           "conductor recieved a message not of type GOTO or MOVE: type %d from %d", req.type, callee);

    GotoResult_t result;
    /* check if goto or just a short move */
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;

        int attemptsLeft = random_range(3, 5);
        while (attemptsLeft > 0) {
            source = TrGetEdge(train);
            if ((node_count = findPath(req.arg3, source, dest, path, 32, &total_distance)) <= 0) {
                error("Conductor: Error: No path to destination %s found, sleeping...", dest->name);
                Delay(random_range(10, 500));
                continue;
            }

            if (path[0] != source->dest) {
                // TODO: only this when stopped
                TrDirection(train);
            }

            int i, base = 0;
            for (i = 0; i < node_count - 1; ++i) {
                if (path[i]->reverse == path[i + 1]) {
                    result = TrGotoAfter(train, &(path[base]), (i - base + 1), 150);

                    switch (result) {
                        case GOTO_COMPLETE:
                            // yay! do nothing
                            break;
                        case GOTO_REROUTE:
                            goto reroute;
                        case GOTO_LOST:
                            goto lost;
                        case GOTO_NONE:
                            // wat
                            ASSERT(false, "GOTO result of GOTO_NONE from train %d (tid %d) on path %s with len %d",
                                   req.arg3, train, path[base]->name, (i - base + 1));
                        default:
                            // waaaaaat
                            ASSERT(false, "This should never happen...");
                    }

                    if (TrDirection(train) < 0) {
                        // failed to reverse?
                        Delay(random_range(1, 500));
                        if (TrDirection(train) < 0) {
                            // second fail, screw it reroute
                            goto reroute;
                        }
                    }
                    base = ++i;
                }
            }

            if (base != node_count) {
                if ((result = TrGotoAfter(train, &(path[base]), node_count - base, 0)) == GOTO_COMPLETE) {
                    // success!
                    break;
                }
            }

reroute:
            Log("Conductor (%d): train %d route failed, rerouting...", myTid, req.arg3);
            attemptsLeft -= 1;
        }
    } else {
        destDist = req.arg2;
        result = TrGotoAfter(train, NULL, 0, destDist);
    }

    if (result == GOTO_COMPLETE) {
        // yay it worked
    } else {

    }

    status = DispatchStopRoute(req.arg3);
    if (status < 0) {
        error("Conductor: Error: Got %d in send to parent %u", status, MyParentTid());
    }
    debug("Conductor: Tid %u, removing self from parent %u", myTid, MyParentTid());
    return;

lost:
    // WE GOTTA GO BACK
    // TODO: msg parent about this, handle in dispatcher
    error("Train is totally lost, currently unhandled!");
    status = DispatchStopRoute(req.arg3);
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
