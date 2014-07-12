#ifndef __PATH_H__ /* __PATH_H__ */
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
int findPath(unsigned int tr, track_node *start, track_node *end, track_node **path, int pathlen, unsigned int *length);

#if TEST
void testHeap();
#endif

#endif /* __PATH_H__ */
