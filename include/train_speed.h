#ifndef __TRAIN_SPEED_H__
#define __TRAIN_SPEED_H__
#include <types.h>

#define TRAIN_COUNT         9

void initTrainSpeeds();
void getTrainIds(int *train_ids);
bool isValidTrainId(unsigned int tr);
int shortmoves(unsigned int tr, unsigned int speed, int dist);
int shortmoves_dist(unsigned int tr, unsigned int speed, unsigned int ticks);
unsigned int getTrainVelocity(unsigned tr, unsigned int sp);
unsigned int getStoppingDistance(unsigned int tr, int startsp, int destsp);
unsigned int getTransitionDistance(unsigned int tr, int startsp, int destsp, int ticks);
unsigned int getTransitionTicks(unsigned int tr, int startsp, int destsp);

#endif /* __TRAIN_SPEED_H__ */
