#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__
#include <track_node.h>


int TrCreate(int priority, int tr, track_node *start_sensor);
int TrSpeed(unsigned int tid, unsigned int speed);
int TrReverse(unsigned int tid);
int TrAuxiliary(unsigned int tid, unsigned int aux);
int LookupTrain(unsigned int tid);
int TrGetSpeed(unsigned int tid);
track_node *TrGetLocation(unsigned int tid, unsigned int *dist);

#endif /* __TRAIN_TASK_H__ */
