#ifndef __CONDUCTOR_H__
#define __CONDUCTOR_H__
#include <track_node.h>


void Conductor();
unsigned int getReverseOffset();
void setReverseOffset(unsigned int offset);
int GoTo(unsigned int tid, unsigned int train, unsigned int tr_number, track_node *sensor, unsigned int distance);
int Move(unsigned int tid, unsigned int train, unsigned int tr_number, unsigned int distance);

#endif /* __CONDUCTOR_H__ */
