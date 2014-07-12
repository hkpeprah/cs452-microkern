#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__
#include <types.h>
#include <track_node.h>

// reserve n tracks OR the specified distance
track_node *reserveTrackDist(uint32_t tr, track_node **track, uint32_t n, uint32_t dist);

// reserve n tracks
track_node *reserveTrack(uint32_t tr, track_node **track, uint32_t n);

// release n tracks
track_node *releaseTrack(uint32_t tr, track_node **track, uint32_t n);

#endif
