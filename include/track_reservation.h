#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__
#include <types.h>
#include <track_node.h>

// reserve n tracks OR the specified distance
// the dist is a ptr and will be modified so the value after the function ends
// is the dist not reserved
// return: # of successfully resv'd nodes
int reserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist);

// reserve n tracks
// return: # of successfully resv'd nodes
int reserveTrack(uint32_t tr, track_node **track, uint32_t n);

// release n tracks
// return: # of successfully release'd nodes
int releaseTrack(uint32_t tr, track_node **track, uint32_t n);

#endif
