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

    GotoResult_t result = GOTO_NONE;

    /* check if goto or just a short move */
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;

        int attemptsLeft;
        for (attemptsLeft = /*random_range(3, 5)*/ 1; attemptsLeft > 0; --attemptsLeft) {
            Log("Routing attempts left: %d\n", attemptsLeft);

            source = TrGetEdge(train);

            if ((node_count = findPath(req.arg3, source, dest, path, 32, &total_distance)) < 0) {
                error("Conductor: Error: No path to destination %s found, sleeping...", dest->name);
                Delay(random_range(10, 500));
                continue;
            }

            Log("Found path of length %d\n", node_count);

            if (path[0] != source->dest) {
                // TODO: only this when stopped
                Log("Conductor %d: initial path reversal for train %d\n", myTid, train);
                TrDirection(train);
            }

            if (node_count >= 2 && path[node_count - 2]->reverse == path[node_count - 1]) {
                --node_count;
            }

            Log("Conductor starting loop\n");

            int i, base = 0;
            for (i = 0; i < node_count - 1; ++i) {
                if (path[i]->reverse == path[i + 1]) {
                    result = TrGotoAfter(train, &(path[base]), (i - base + 1), 175);

                    Log("\n<><><><><>Finished PARTIAL route %s -> %s with result %d\n", path[base]->name, path[i]->name, result);

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

                    if (i < node_count - 1 && TrDirection(train) < 0) {
                        // failed to reverse?
                        Delay(random_range(1, 500));
                        if (TrDirection(train) < 0) {
                            // second fail, screw it reroute
                            goto reroute;
                        }
                    }
                    Log("Conductor rv train {%d} complete\n", req.arg3);
                    base = ++i;
                }
            }

            Log("Conductor finished partial path loop, starting final loop. base: %d, i: %d\n", base, i);

            if (base != node_count) {
                if ((result = TrGotoAfter(train, &(path[base]), node_count - base, 0)) == GOTO_COMPLETE) {
                    Log("\n<><><><><>Finished FINAL route %s -> %s with result %d\n", d(path[base]).name, d(path[node_count - 1]).name, result);
                    source = TrGetEdge(train);
                    Log("Train edge: %s\n", d(d(source).src).name);
                    if (source->src == dest || source->src->reverse == dest) {
                        // success!
                        Log("Success route!\n");
                        break;
                    }
                    // otherwise, reroute
                    Log("Fail and rerouting?!\n");
                }
            } else {
                break;
            }

reroute:
            debug("Conductor: train route failed, rerouting...");
        }
    } else {
        destDist = req.arg2;
        result = TrGotoAfter(train, NULL, 0, destDist);
    }

    if (result == GOTO_COMPLETE) {
        Log("goto complete successfully\n");
    } else {
        Log("goto failed as %d\n", result);
    }

    status = DispatchStopRoute(req.arg3);
    if (status < 0) {
        error("Conductor: Error: Got %d in send to parent %u", status, MyParentTid());
    }
    debug("Conductor: Tid %u, removing self from parent %u", myTid, MyParentTid());
    return;

lost:
    DispatchAddTrain(req.arg3);
    return;
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
