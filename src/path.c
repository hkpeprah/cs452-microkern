#include <path.h>
#include <track_data.h>
#include <stdlib.h>
#include <term.h>

#define REVERSE_DIST   100
#define DIR_REVERSE    2
#ifndef RAW_DEBUG
#define RAW_DEBUG      0
#endif

#define CONVERT_SWITCH(x)    (x >= 153 ? x - 134 : x)
#define LEFT(x)              ((x << 1) + 1)
#define RIGHT(x)             ((x << 1) + 2)
#define PARENT(x)            ((x - 1) >> 1)


typedef struct Node {
    track_node *trackNode;
    struct Node *pred;
    unsigned int heapIndex : 8;
    unsigned int dist : 15;
    unsigned int pathLenNodes : 8;
    unsigned int inHeap : 1;
} Node_t;


static inline int trackNodeToIndex(track_node *node) {
    switch(node->type) {
        case NODE_SENSOR:
            return node->num;
        case NODE_BRANCH:
            return 80 + ((CONVERT_SWITCH(node->num) - 1) << 1);
        case NODE_MERGE:
            return 81 + ((CONVERT_SWITCH(node->num) - 1) << 1);
        case NODE_ENTER:
        case NODE_EXIT:
            return node->num;
        default:
            kprintf("PATH: FATAL: trackNodeToIndex hit default");
            return -1;
    }
}


typedef struct Heap {
    Node_t *arr[TRACK_MAX];
    unsigned int size;
} Heap_t;


static void swap(Node_t **a, Node_t **b) {
    Node_t *tmp = *a;
    *a =  *b;
    *b = tmp;

    unsigned int tmpIndex = (*a)->heapIndex;
    (*a)->heapIndex = (*b)->heapIndex;
    (*b)->heapIndex = tmpIndex;
}


static void heapify(Heap_t *heap) {
    int index = 0;
    int minIndex, left, right;

    if (heap->size == 0) {
        return;
    }

    while (1) {
        left = LEFT(index);
        right = RIGHT(index);
        minIndex = index;

        minIndex = (left < heap->size
                        && heap->arr[left]->dist < heap->arr[minIndex]->dist) ? left : minIndex;
        minIndex = (right < heap->size
                        && heap->arr[right]->dist < heap->arr[minIndex]->dist) ? right : minIndex;

        if (minIndex == index) {
            break;
        }

        swap(&heap->arr[index], &heap->arr[minIndex]);
        index = minIndex;
    }
}


static void heapifyUp(Heap_t *heap, Node_t *node) {
    int parent, index;
    while(1) {
        index = node->heapIndex;
        parent = PARENT(index);

        if (parent < 0 || heap->arr[parent]->dist <= heap->arr[index]->dist) {
            break;
        }

#if RAW_DEBUG
        kprintf("me: %d dist {%d}, parent: %d dist {%d}\n",
                index, heap->arr[index]->dist, parent, heap->arr[parent]->dist);
#endif 

        swap(&heap->arr[index], &heap->arr[parent]);
    }
}


static void heapInsert(Heap_t *heap, Node_t *val) {
    val->heapIndex = heap->size++;
    val->inHeap = 1;
    heap->arr[val->heapIndex] = val;
    heapifyUp(heap, val);
}


static Node_t *heapPop(Heap_t *heap) {
    if (heap->size == 0) {
        error("HEAP is empty");
        return NULL;
    }
    Node_t *res = heap->arr[0];
    swap(&heap->arr[0], &heap->arr[heap->size - 1]);
    heap->size -= 1;
    res->heapIndex = 0;
    res->inHeap = 0;
    heapify(heap);
    return res;
}


void dumpHeap(Heap_t *heap) {
    int i;
    debug("size: (%d) ", heap->size);
    for (i = 0; i < heap->size; ++i) {
        debug("%d ", heap->arr[i]->dist);
    }
    debug("\n");
}


// given the current node and a position to go (straight, curved, or reverse), compute
// the distance, update it if the new distance is shorter, and restructure the heap
static void addNode(Heap_t *heap, Node_t *nodes, Node_t *currentNode, unsigned int direction) {
    track_node *currentTrackNode, *nextTrackNode;
    Node_t *nextNode;
    unsigned int edgeDist;

    currentTrackNode = currentNode->trackNode;

    switch (direction) {
        case DIR_STRAIGHT:
        case DIR_CURVED:
            nextTrackNode = currentTrackNode->edge[direction].dest;
            edgeDist = currentTrackNode->edge[direction].dist;
            break;
        case DIR_REVERSE:
            nextTrackNode = currentTrackNode->reverse;
            edgeDist = REVERSE_DIST;
            break;
        default:
            error("Pathfinding: addNode: incorrect direction %d", direction);
            return;
    }

    nextNode = &nodes[trackNodeToIndex(nextTrackNode)];

    if (currentNode->dist + edgeDist < nextNode->dist) {
        nextNode->trackNode = nextTrackNode;
        nextNode->dist = currentNode->dist + edgeDist;
        nextNode->pred = currentNode;
        nextNode->pathLenNodes = currentNode->pathLenNodes + 1;

        if (nextNode->inHeap) {
            heapifyUp(heap, nextNode);
        } else {
            heapInsert(heap, nextNode);
        }
#if RAW_DEBUG
        kprintf("added new node with name %s, distance %d, pred %s\n",
                nextTrackNode->name, nextNode->dist, currentTrackNode->name);
        dumpHeap(heap);
#endif
    }
}


int findPath(unsigned int tr, track_node *start, track_node *end, track_node **path, int pathlen, unsigned int *length) {
    int i;
    Node_t nodes[TRACK_MAX] = {{0}};
    Heap_t heap = {.arr = {0}, .size = 0};

    Node_t *currentNode;
    track_node *currentTrackNode;
    unsigned int index;
    unsigned int finalPathLen;

    for (i = 0; i < TRACK_MAX; ++i) {
        nodes[i].trackNode = NULL;
        nodes[i].pred = NULL;
        nodes[i].heapIndex = 0;
        nodes[i].dist = (1 << 15) - 1;
        nodes[i].pathLenNodes = 0;
        nodes[i].inHeap = 0;
    }

    index = trackNodeToIndex(start);
    nodes[index].trackNode = start;
    nodes[index].dist = 0;
    nodes[index].pathLenNodes = 1;

    nodes[trackNodeToIndex(end)].trackNode = end;

    heapInsert(&heap, &nodes[index]);

    while (heap.size > 0) {
        // get head of heap
        currentNode = heapPop(&heap);
        currentTrackNode = currentNode->trackNode;
#if RAW_DEBUG
        kprintf("processing node %s dist %d\n", currentTrackNode->name, currentNode->dist);
        bwgetc(COM2);
#endif
        if (currentTrackNode == end) {
            break;
        }

        if (currentTrackNode->reservedBy != RESERVED_BY_NOBODY && currentTrackNode->reservedBy != tr) {
            // someone else owns that node - skip it!
            continue;
        }

        switch (currentTrackNode->type) {
            case NODE_BRANCH:
                addNode(&heap, nodes, currentNode, DIR_CURVED);
            case NODE_MERGE:
            case NODE_SENSOR:
            case NODE_ENTER:
                //addNode(&heap, nodes, currentNode, DIR_REVERSE);
                addNode(&heap, nodes, currentNode, DIR_STRAIGHT);
                break;
            default:
                break;
        }
    }

    currentNode = &nodes[trackNodeToIndex(end)];
    *length = currentNode->dist;
    finalPathLen = currentNode->pathLenNodes;

    if (finalPathLen > pathlen) {
        error("PATH: Supplied array not long enough");
        return finalPathLen;
    }

    i = finalPathLen;
    path[i] = NULL;

    while (i --> 0) {
        path[i] = currentNode->trackNode;

        if (currentNode->trackNode == start || currentNode->pred == NULL) {
            break;
        }

        currentNode = currentNode->pred;
    }

    return finalPathLen;
}

#if TEST
void testHeap() {
    int i;
    int dists1[] = {8, 3, 4, 1, 5, 11, 2, 6, 7, 12, 9, 10};

    Node_t nodes[12] = {{0}};
    Heap_t heap = {.arr = {0}, .size = 0};

    Node_t *node;

    for (i = 0; i < 6; ++i) {
        nodes[i].dist = dists1[i];
        heapInsert(&heap, &nodes[i]);
        dumpHeap(&heap);

    }

    for (i = 0; i < 3; ++i) {
        node = heapPop(&heap);
        kprintf("got %d\n", node->dist);
        dumpHeap(&heap);
    }

    for (i = 6; i < 12; ++i) {
        nodes[i].dist = dists1[i];
        heapInsert(&heap, &nodes[i]);
        dumpHeap(&heap);
    }

    nodes[10].dist = 0;
    heapifyUp(&heap, &nodes[10]);

    for (i = 0; i < 9; ++i) {
        node = heapPop(&heap);
        kprintf("got %d\n", node->dist);
        dumpHeap(&heap);
    }
}
#endif
