#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__
#include <track_node.h>



int TrCreate(int priority, int tr, track_edge *start);
int TrSpeed(unsigned int tid, unsigned int speed);
int TrGoTo(unsigned int tid, track_node *finalDestination);
int TrReverse(unsigned int tid);
int TrAuxiliary(unsigned int tid, unsigned int aux);
int TrGetLocation(unsigned int tid, track_edge **edge, unsigned int *edgeDistMM);
int LookupTrain(unsigned int tid);
int TrGetSpeed(unsigned int tid);

#endif /* __TRAIN_TASK_H__ */
