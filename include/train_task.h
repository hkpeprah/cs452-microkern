#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__

#include <track_node.h>

int TrCreate(int, int, track_edge*);
int TrSpeed(unsigned int, unsigned int);
int TrAuxiliary(unsigned int, unsigned int);
int TrReverse(unsigned int);
int TrGetLocation(unsigned int, track_edge **edge, unsigned int *edgeDistMM);
// train speed in micrometers per clock tick
int TrGetSpeed(int tid);
int LookupTrain(unsigned int);

#endif /* __TRAIN_TASK_H__ */
