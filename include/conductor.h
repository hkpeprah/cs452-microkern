#ifndef __CONDUCTOR_H__
#define __CONDUCTOR_H__
#include <track_node.h>

void Conductor();
int GoTo(unsigned int tid, unsigned int train, unsigned int tr_number, track_node *sensor, unsigned int distance);

#endif /* __CONDUCTOR_H__ */
