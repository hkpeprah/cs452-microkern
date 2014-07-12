#ifndef __TRACK_RESERVATION_H__
#define __TRACK_RESERVATION_H__
#include <track_node.h>

track_node *reserveTrack(unsigned int tr, track_node **track, unsigned int n);
track_node *releaseTrack(unsigned int tr, track_node **track, unsigned int n);

#endif
