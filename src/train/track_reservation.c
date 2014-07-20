#include <track_reservation.h>
#include <term.h>

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
static int swapTrackResvBy(int oldValue, int newValue, track_node **track, uint32_t n, int *dist) {
    track_node *currentNode = *track++;
    track_node *nextNode;
    bool validRes;
    int nextEdgeDist;
    int numSuccess = 0;

    while (currentNode && (validRes = cmpAndSwapResvBy(currentNode, oldValue, newValue))) {
        // debug("%d res %s, %d node %d dist left", newValue, currentNode->name, n, *dist);

        if (!validRes) {
            // node was already taken or tried to free something belonging to someone else
            debug("%s res fail, expected %d but got oldValue %d -> newValue %d", currentNode->name, currentNode->reservedBy, oldValue, newValue);
            break;
        }
        numSuccess++;

        if (--n <= 0) {
            // no more nodes in the array
            break;
        }

        nextNode = *track++;
        if ( (nextEdgeDist = validNextNode(currentNode, nextNode)) < 0 ) {
            // path is not contiguous (not DIR_AHEAD/DIR_STRAIGHT or DIR_CURVED)
            // error("Incorrect path at %s to %s", currentNode->name, nextNode->name);
            break;
        }

        if (*dist <= 0) {
            // debug("out of distance with dist = %d", *dist);
            break;
        }

        *dist -= nextEdgeDist;
        currentNode = nextNode;
    }

    return numSuccess;
}

int reserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, dist);
}

int reserveTrack(uint32_t tr, track_node **track, uint32_t n) {
    int maxint = MAXSINT;
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, &maxint);
}

int releaseTrack(uint32_t tr, track_node **track, uint32_t n) {
    int maxint = MAXSINT;
    return swapTrackResvBy(tr, RESERVED_BY_NOBODY, track, n, &maxint);
}

