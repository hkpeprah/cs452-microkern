#include <track_reservation.h>
#include <types.h>
#include <term.h>

static bool validNextNode(track_node *current, track_node *next) {
    switch(current->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
            return current->edge[DIR_AHEAD].dest == next;
        case NODE_BRANCH:
            return current->edge[DIR_STRAIGHT].dest == next
                    || current->edge[DIR_CURVED].dest == next;
        case NODE_NONE:
        case NODE_EXIT:
        default:
            return false;
    }
}

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
    return false;
}

// runs cmpAndSwapResvBy on each track, provided they are contineous
static track_node *swapTrackResvBy(int oldValue, int newValue, track_node **track, unsigned int n) {
    track_node *lastSuccessNode = NULL;
    track_node *currentNode = *track++;
    track_node *nextNode;
    bool validRes;

    while (currentNode && (validRes = cmpAndSwapResvBy(currentNode, oldValue, newValue))) {
        debug("set %s to %d, %d remaining", currentNode->name, newValue, n);

        if (!validRes) {
            break;
        }

        lastSuccessNode = currentNode;

        if (--n <= 0) {
            break;
        }

        nextNode = *track++;
        if (!validNextNode(currentNode, nextNode)) {
            error("Incorrect path at %s to %s", currentNode->name, nextNode->name);
            break;
        }

        currentNode = nextNode;
    }

    return currentNode;
}

track_node *reserveTrack(unsigned int tr, track_node **track, unsigned int n) {
    return swapTrackResvBy(RESERVED_BY_NOBODY, tr, track, n);
}

track_node *releaseTrack(unsigned int tr, track_node **track, unsigned int n) {
    return swapTrackResvBy(tr, RESERVED_BY_NOBODY, track, n);
}

