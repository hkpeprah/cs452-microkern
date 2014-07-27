#ifndef __PATH_H__ /* __PATH_H__ */
#define __PATH_H__
#include <track_node.h>

#define INSUFFICIENT_SUPPLIED_ARRAY_SIZE    -1
#define DEST_CURRENTLY_RESERVED             -2

/*
 * start    - starting node
 * end      - end node
 * path     - output parameter, list of nodes
 * pathlen  - maxlen of path array
 * length   - length of path (mm)
 *
 * return: actual size of path array, or error
 */
int findPath(unsigned int tr, track_edge *start, track_node *end, track_node **path, int pathlen, unsigned int *length);

#if TEST
void testHeap();
#endif

#endif /* __PATH_H__ */
