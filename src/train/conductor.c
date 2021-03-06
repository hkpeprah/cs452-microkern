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
#include <types.h>
#include <transit.h>

// how many MM to go past a target for the purpose of reversing
static volatile unsigned int RV_OFFSET = 290;

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


void setReverseOffset(unsigned int offset) {
    RV_OFFSET = offset;
}


unsigned int getReverseOffset() {
    return RV_OFFSET;
}


static bool isTrainAtDest(int train, track_node *dest, int expectOffset, int tr_number) {
    uint32_t offset = 0;
    track_node *node;
    track_edge *edge;
    ASSERT((node = TrGetLocation(train, &offset)), "Null node?!");
    ASSERT((edge = getNextEdge(node)), "Null edge?!");

    debug("Conductor: Train %d (Tid %d) at %d past %s, destination is %d past %s", tr_number, train,
          offset, d(node).name, expectOffset, d(dest).name);

    if (expectOffset != 0) {
        return (edge->src == dest && ABS(expectOffset - offset) < MAX_NODE_OFFSET);
    }

    return (((edge->src == dest || edge->src->reverse == dest) && offset < MAX_NODE_OFFSET));
}


void Conductor() {
    ConductorMessage_t req;
    track_edge *source;
    track_node *dest, *path[32] = {0};
    int callee, status, myTid, train;
    int node_count, tr_number;
    unsigned int total_distance, destDist, reverse_offset;

    status = Receive(&callee, &req, sizeof(req));
    if (status < 0) {
        error("Error: Error in send, received %d from %d", status, callee);
        Reply(callee, NULL, 0);
        Exit();
    }
    status = 1;
    Reply(callee, &status, sizeof(status));

    train = req.arg0;
    destDist = req.arg2;
    tr_number = req.arg3;
    myTid = MyTid();
    dest = NULL;
    reverse_offset = getReverseOffset();

    ASSERT((req.type == GOTO || req.type == MOVE),
           "recieved a message not of type GOTO or MOVE: type %d from %d", req.type, callee);

    /* check if goto or just a short move */
    GotoResult_t result = GOTO_NONE;
    if (req.type == GOTO) {
        int attemptsLeft;
        dest = (track_node*)req.arg1;
        for (attemptsLeft = 10; attemptsLeft > 0; --attemptsLeft) {
            source = TrGetEdge(train);
            if (source == NULL) {
                error("Train %d (Tid %d) reported NULL edge", tr_number, train);
                goto lost;
            }

            if (isTrainAtDest(train, dest, destDist, tr_number)) {
                goto done;
            }

            node_count = 0;
            if ((node_count = findPath(tr_number, source, dest, path, 32, &total_distance)) <= 0) {
                error("Error: No path to destination %s found, sleeping...", dest->name);
                goto reroute;
            }

            if (path[0] != source->dest) {
                while (TrDirection(train) < 0) {
                    TrSpeed(train, 0);
                    Delay(600); /* maximum possible time to wait for a train to finish moving */
                }
            }

            if (node_count >= 2 && path[node_count - 2]->reverse == path[node_count - 1]) {
                // TODO: this doesn't correctly handle the "GOTO-AFTER" case. should reduce the
                // final node 1 before and use a dist offset of edgeDist - requested offset
                --node_count;
            }

            int i, base = 0;
            for (i = 0; i < node_count - 1; ++i) {
                if (path[i] && path[i]->reverse == path[i + 1]) {
                    Log("Conductor for Train %d: Exec PARTIAL route %s -> %s", tr_number, path[base]->name, path[i]->name);
                    result = TrGotoAfter(train, &(path[base]), (i - base + 1), reverse_offset);
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
                        Delay(random_range(200, 500));
                        if (TrDirection(train) < 0) {
                            /* if we've failed again, assume we can't reserve the reverse node */
                            goto reroute;
                        }
                    }
                    base = i + 1;
                }
            }

            if (base != node_count) {
                Log("Exec FINAL route %s -> %s", d(path[base]).name, d(path[node_count - 1]).name);
                result = TrGotoAfter(train, &(path[base]), node_count - base, 0);
                switch (result) {
                    case GOTO_COMPLETE:
                        // check that the train is within 5 cm of dest
                        if (isTrainAtDest(train, dest, destDist, tr_number)) {
                            // success!
                            goto done;
                        }
                        // fallthrough to reroute
                    case GOTO_REROUTE:
                        break;
                    case GOTO_LOST:
                        goto lost;
                    case GOTO_NONE:
                        ASSERT(false, "GOTO result of GOTO_NONE from train %d (tid %d) on path %s with len %d",
                                tr_number, train, path[base]->name, (i - base + 1));
                        break;
                    default:
                        ASSERT(false, "This should never happen...");
                }
            } else {
                break;
            }
reroute:
            debug("Conductor (Tid %d): Re-routing train %d after sleep...", myTid, tr_number);
            Delay(random_range(100, 500));
            continue;
lost:
            /* we're lost, so let's re-add the train */
            debug("Conductor (Tid %d): Train %u is lost", myTid, tr_number);
            TrDelete(train);
            train = DispatchReAddTrain(tr_number);
            if (train < 0) {
                error("Conductor: Error: Tried to add back train %d, but got %d", tr_number, train);
                break;
            }
            Delay(200);
        }
        /* when the conductor gives up, we have to clean up the mess we made */
        notice("Conductor (Tid %d): Giving up on routing train %d (Tid %d)", myTid, tr_number, train);
        TrDelete(train);
        train = DispatchReAddTrain(tr_number);
        if (train < 0) {
            error("Conductor: Error: Tried to add back train %d, but got %d", tr_number, train);
        }
    } else {
        /* making a generic distance move */
        result = TrGotoAfter(train, NULL, 0, destDist);
    }

    /* either the GOTO has been completed here, or we have failed in the Goto, either
       way, we're not exiting */
done:
    /* remove self from parent, so that train can be used again */
    if (dest != NULL) {
        Log("Conductor (Tid %d): Broadcasting arrival of train %d at sensor %s (%d)", myTid, tr_number, dest->name, dest->num);
        Broadcast(tr_number, dest->num);
    }
    status = DispatchStopRoute(tr_number);
    if (status < 0) {
        error("Conductor (Tid %d): Error: Got %d in send to parent %u", myTid, status, MyParentTid());
    }
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
