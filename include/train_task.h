#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__
#include <track_node.h>

int TrCreate(int priority, int tr, track_node *start_sensor);
int TrSpeed(unsigned int tid, unsigned int speed);
int TrDir(unsigned int tid);
int TrReverse(unsigned int tid);
int TrDirection(unsigned int tid);
int TrAuxiliary(unsigned int tid, unsigned int aux);
int TrGetSpeed(unsigned int tid);
int TrGotoAfter(unsigned int tid, track_node **path, unsigned int pathLen, unsigned int dist);
track_node *TrGetLocation(unsigned int tid, unsigned int *dist);
track_edge *TrGetEdge(unsigned int tid);

#endif /* __TRAIN_TASK_H__ */
