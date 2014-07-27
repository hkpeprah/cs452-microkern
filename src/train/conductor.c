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
    unsigned int node_count, tr_number;
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
    tr_number = req.arg3;
    myTid = MyTid();

    ASSERT((req.type == GOTO || req.type == MOVE),
           "conductor recieved a message not of type GOTO or MOVE: type %d from %d", req.type, callee);

    /* check if goto or just a short move */
    GotoResult_t result = GOTO_NONE;
    if (req.type == GOTO) {
        dest = (track_node*)req.arg1;
        destDist = req.arg2;

        int attemptsLeft;
        for (attemptsLeft = random_range(3, 5); attemptsLeft > 0; --attemptsLeft) {
            Log("Routing attempts left: %d\n", attemptsLeft);

            source = TrGetEdge(train);

            if (source == NULL) {
                Log("Train %d reported NULL edge, readding...\n", train);
                goto lost;
            }

            Log("Routing from train edge %s -> %s to %s\n", (source ? source->src->name : "NULL"), (source ? source->dest->name : "NULL"), dest->name);

            if ((node_count = findPath(tr_number, source, dest, path, 32, &total_distance)) < 0) {
                error("Conductor: Error: No path to destination %s found, sleeping...", dest->name);
                Delay(random_range(10, 500));
                continue;
            }

            Log("Found path of length %d\n", node_count);

            if (path[0] != source->dest) {
                Log("Conductor %d: initial path reversal for train %d\n", myTid, train);
                while (TrDirection(train) < 0) {
                    TrSpeed(train, 0);
                    Delay(600); /* maximum possible time to wait for a train to finish moving */
                }
            }

            if (node_count >= 2 && path[node_count - 2]->reverse == path[node_count - 1]) {
                --node_count;
            }

            Log("Conductor starting loop\n");

            int i, base = 0;
            for (i = 0; i < node_count - 1; ++i) {
                Log("path node: %s\n", d(path[i]).name);
                if (path[i] && path[i]->reverse == path[i + 1]) {
                    Log("\n<><><><><>Exec PARTIAL route %s -> %s with result %d\n",
                            path[base]->name, path[i]->name, result);
                    result = TrGotoAfter(train, &(path[base]), (i - base + 1), 300);

                    switch (result) {
                        case GOTO_COMPLETE:
                            break;
                        case GOTO_REROUTE:
                            goto reroute;
                        case GOTO_LOST:
                            goto lost;
                        case GOTO_NONE:
                            /* should never reach this case */
                            ASSERT(false, "GOTO result of GOTO_NONE from train %d (tid %d) on path %s with len %d",
                                   tr_number, train, path[base]->name, (i - base + 1));
                        default:
                            /* this case is even worse, this should never happen */
                            ASSERT(false, "This should never happen...");
                    }

                    if (i < node_count - 1 && TrDirection(train) < 0) {
                        /* failed to reverse, so wait and see if we can shortly */
                        Delay(random_range(1, 500));
                        if (TrDirection(train) < 0) {
                            /* if we've failed again, assume we can't reserve the reverse node */
                            goto reroute;
                        }
                    }
                    Log("Conductor: reversed train %d\n", tr_number);
                    base = i + 1;
                }
            }

            Log("Conductor finished partial path loop, starting final loop. base: %d, i: %d\n", base, i);
            if (base != node_count) {
                Log("\n<><><><><>Exec FINAL route %s -> %s ",
                        d(path[base]).name, d(path[node_count - 1]).name);

                result = TrGotoAfter(train, &(path[base]), node_count - base, 0);

                switch (result) {
                    case GOTO_COMPLETE:
                        Log("result = GOTO_COMPLETE\n");
                        source = TrGetEdge(train);
                        Log("Train edge: %s\n", d(d(source).src).name);
                        if (source->src == dest || source->src->reverse == dest) {
                            // success!
                            Log("Success route!\n");
                            goto done;
                        } else {
                            // reroute
                            if (source->dest == dest || source->dest->reverse == dest) {
                                // case 1: we are on right edge but just need to go a little bit more.
                                // "nudge" it with a series of short moves
                                int tries;
                                for (tries = 5; tries > 0 && (source->src != dest && source->src->reverse != dest); --tries) {
                                    result = TrGotoAfter(train, &(dest), 1, 50);
                                    source = TrGetEdge(train);
                                }
                                if (source->src == dest || source->src->reverse == dest) {
                                    Log("Success after nudging!\n");
                                    goto done;
                                }
                                Log("Nudging failed :'(\n");
                                // 5 tries failed to get to dest OR not on edge to dest -> reroute
                                goto reroute;
                            } else {
                                goto lost;
                            }
                        }
                    case GOTO_LOST:
                        Log("result = GOTO_LOST\n");
                        goto lost;
                    case GOTO_REROUTE:
                        Log("result = GOTO_REROUTE\n");
                        break;
                    case GOTO_NONE:
                        // wat
                        ASSERT(false, "GOTO result of GOTO_NONE from train %d (tid %d) on path %s with len %d",
                                tr_number, train, path[base]->name, (i - base + 1));
                    default:
                        // waaaaaat
                        ASSERT(false, "This should never happen...");
                }
            } else {
                break;
            }
reroute:
            debug("Conductor: Routed failed for train %d, re-routing", tr_number);
            continue;
lost:
            /* we're lost, so let's re-add the train */
            debug("Conductor: Train %d is lost, re-adding", tr_number);
            train = DispatchReAddTrain(tr_number);
        }
    } else {
        /* making a generic distance move */
        destDist = req.arg2;
        result = TrGotoAfter(train, NULL, 0, destDist);
    }

    /* either the GOTO has been completed here, or we have failed in the Goto, either
       way, we're not exiting */
done:
    /* remove self from parent, so that train can be used again */
    status = DispatchStopRoute(tr_number);
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
