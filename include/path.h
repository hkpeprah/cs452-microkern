#ifndef __PATH_H__
#define __PATH_H__

#include <track_node.h>

/*
 * start    - starting node
 * end      - end node
 * path     - output parameter, list of nodes
 * pathlen  - maxlen of path
 * used     - array of nodes that are being used
 * usedlen  - size of array
 *
 * return: actual size of path
 */
int findPath(track_node *start, track_node *end, track_node **path, int pathlen, track_node **used, int usedlen);

#if TEST
void testHeap();
#endif

#endif
