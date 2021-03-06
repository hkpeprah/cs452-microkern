#ifndef __TRACK_NODE_H__
#define __TRACK_NODE_H__

#define DIR_AHEAD             0
#define DIR_STRAIGHT          0
#define DIR_CURVED            1
#define RESERVED_BY_NOBODY    -1
#define INVALID_NEXT_NODE     -2
#define NULL_NODES            -3
#define REVERSE_DIST          100

typedef enum {
    NODE_NONE,
    NODE_SENSOR,
    NODE_BRANCH,
    NODE_MERGE,
    NODE_ENTER,
    NODE_EXIT,
} node_type;

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge track_edge;

struct track_edge {
    track_edge *reverse;
    track_node *src, *dest;
    int dist;             /* in millimetres */
};

struct track_node {
    const char *name;
    node_type type;
    int num;              /* sensor or switch number */
    track_node *reverse;  /* same location, but opposite direction */
    track_edge edge[2];
    int reservedBy;       /* train # of train that reserved this node */
};


// INVALID_NEXT_NODE when invalid
// distance of edge when valid
int validNextNode(track_node *current, track_node *next);

// NULL when invalid (ie. exit node)
// ptr to next edge when valid
track_edge *getNextEdge(track_node *node);

#endif
