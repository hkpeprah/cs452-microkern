#include <track_reservation.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>


// compare oldValue with existing. If true, swap to new value
// returns true on success, false on failure
static bool cmpAndSwapResvBy(track_node *node, int oldValue, int newValue) {
    if (node->reservedBy == oldValue && node->reverse->reservedBy == oldValue) {
        node->reservedBy = newValue;
        node->reverse->reservedBy = newValue;
        return true;
    }

    return (node->reverse->reservedBy == newValue && node->reservedBy == newValue);
}


// runs cmpAndSwapResvBy on each track, provided they are contineous
static int swapTrackResvBy(int oldValue, int newValue, track_node **track, int n, int *dist) {
    ASSERT(track && n > 0, "Expected non-empty array, instead got %s, %d", (track ? track[0]->name : "NULL"), n);

    track_node *currentNode = *track++;
    track_node *nextNode;
    track_edge *nextEdge;
    bool validRes;
    int nextEdgeDist;
    int numSuccess = 0;

    while (currentNode && (validRes = cmpAndSwapResvBy(currentNode, oldValue, newValue))) {
        // debug("%d res %s, %d node %d dist left", newValue, d(currentNode).name, n, (dist ? *dist : -1));
        numSuccess++;

        if (--n <= 0) {
            if (dist) {
                // no more nodes left but still has dist, just use track state
                if ((nextEdge = getNextEdge(currentNode))) {
                    nextNode = nextEdge->dest;
                } else {
                    // next node is null! nothing to see here
                    return numSuccess;
                }
                //debug("using next node from track state %s", d(nextNode).name);
            } else {
                //debug("no nodes left, no dist left, break");
                break;
            }
        } else {
            // still has node left
            nextNode = *track++;
        }

        nextEdgeDist = validNextNode(currentNode, nextNode);
        // path is not contiguous (not DIR_AHEAD/DIR_STRAIGHT or DIR_CURVED)
        ASSERT(nextEdgeDist >= 0, "Incorrect path at %s to %s",
                (currentNode ? currentNode->name : "NULL"), (nextNode ? nextNode->name : "NULL"));

        if (dist) {
            if (*dist <= 0) {
                //debug("out of distance with dist = %d", *dist);
                break;
            } else {
                //debug("has dist, subtracting nextEdgeDist: %d - %d", *dist, nextEdgeDist);
                *dist -= nextEdgeDist;
            }
        }
        currentNode = nextNode;
    }

    return numSuccess;
}


int reserveTrackDist(uint32_t tr, track_node **track, int n, int *dist) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, dist);
}


int reserveTrack(uint32_t tr, track_node **track, int n) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, NULL);
}


int releaseTrack(uint32_t tr, track_node **track, int n) {
    return swapTrackResvBy(tr, RESERVED_BY_NOBODY, track, n, NULL);
}

