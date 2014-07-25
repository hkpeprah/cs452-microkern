#include <track_node.h>
#include <train.h>
#include <stdlib.h>

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
            } else if (current->reverse == next) {
                return REVERSE_DIST;
            }
            return INVALID_NEXT_NODE;
        case NODE_NONE:
        case NODE_EXIT:
        default:
            return INVALID_NEXT_NODE;
    }
}


track_edge *getNextEdge(track_node *node) {
    Switch_t *swtch;
    switch (d(node).type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
            return &(d(node).edge[DIR_AHEAD]);
        case NODE_BRANCH:
            swtch = getSwitch(node->num);
            return &(d(node).edge[d(swtch).state]);
        case NODE_EXIT:
            return NULL;
        case NODE_NONE:
        default:
            return NULL;
    }
}
