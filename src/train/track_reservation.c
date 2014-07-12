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

    error("Node %s cmpAndSwapResvBy failed: rsv {%d}, reverse rsv {%d}, oldValue {%d}, newValue {%d}",
            node->reservedBy, node->reverse->reservedBy, oldValue, newValue);
    return (node->reverse->reservedBy == newValue && node->reservedBy == newValue);
}


// runs cmpAndSwapResvBy on each track, provided they are contineous
static track_node *swapTrackResvBy(int oldValue, int newValue, track_node **track, uint32_t n, uint32_t dist) {
    track_node *lastSuccessNode = NULL;
    track_node *currentNode = *track++;
    track_node *nextNode;
    bool validRes;
    int nextEdgeDist;

    while (currentNode && (validRes = cmpAndSwapResvBy(currentNode, oldValue, newValue))) {
        debug("set %s to %d, %d remaining", currentNode->name, newValue, n);

        if (!validRes) {
            // node was already taken or tried to free something belonging to someone else
            break;
        }

        lastSuccessNode = currentNode;

        if (--n <= 0) {
            // no more nodes in the array
            break;
        }

        nextNode = *track++;
        if ( (nextEdgeDist = validNextNode(currentNode, nextNode)) < 0 ) {
            // path is not contiguous (not DIR_AHEAD/DIR_STRAIGHT or DIR_CURVED)
            error("Incorrect path at %s to %s", currentNode->name, nextNode->name);
            break;
        }

        dist -= nextEdgeDist;
        if (dist <= 0) {
            // ran out of distance
            break;
        }

        currentNode = nextNode;
    }

    return lastSuccessNode;
}

track_node *reserveTrackDist(uint32_t tr, track_node **track, uint32_t n, uint32_t dist) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, dist);
}

track_node *reserveTrack(uint32_t tr, track_node **track, uint32_t n) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n, MAXUINT);
}

track_node *releaseTrack(uint32_t tr, track_node **track, uint32_t n) {
    return swapTrackResvBy(tr, RESERVED_BY_NOBODY, track, n, MAXUINT);
}

