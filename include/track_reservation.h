#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__
#include <types.h>
#include <track_node.h>

// reserve n tracks OR the specified distance
// the dist is a ptr and will be modified so the value after the function ends
// is the dist not reserved
track_node *reserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist);

// reserve n tracks
track_node *reserveTrack(uint32_t tr, track_node **track, uint32_t n);

// release n tracks
track_node *releaseTrack(uint32_t tr, track_node **track, uint32_t n);

#endif
