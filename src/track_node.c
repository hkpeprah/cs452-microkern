#include <track_node.h>

// INVALID_NEXT_NODE on invalid
// edge distance on valid
int validNextNode(track_node *current, track_node *next) {
    track_edge *straight = &(current->edge[DIR_AHEAD]);

    switch(current->type) {
        case NODE_BRANCH:
            if (current->edge[DIR_CURVED].dest == next) {
                return current->edge[DIR_CURVED].dist;
            }
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
            if (straight->dest == next) {
                return straight->dist;
            }
            return INVALID_NEXT_NODE;
        case NODE_NONE:
        case NODE_EXIT:
        default:
            return INVALID_NEXT_NODE;
    }
}
