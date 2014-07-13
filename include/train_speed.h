#ifndef __TRAIN_SPEED_H__
#define __TRAIN_SPEED_H__
#include <types.h>

struct train_speed_state {
    const char *train;
    unsigned int ticks[15];
    unsigned int speed[15];
    unsigned int stoppingDistances[15];
    unsigned int stoppingTicks[15];
};


void initTrainSpeeds();
bool isValidTrainId(unsigned int tr);
unsigned int shortmoves(unsigned int tr, unsigned int speed, int dist);
unsigned int getTrainVelocity(unsigned tr, unsigned int sp);
unsigned int getStoppingDistance(unsigned int tr, int startsp, int destsp);
unsigned int getTransitionDistance(unsigned int tr, int startsp, int destsp, int ticks);
unsigned int getTransitionTicks(unsigned int tr, int startsp, int destsp);

#endif /* __TRAIN_SPEED_H__ */
