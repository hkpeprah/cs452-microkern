#ifndef __TRAIN_SPEED_H__
#define __TRAIN_SPEED_H__
#include <types.h>

struct train_speed_state {
    const char *train;
    unsigned int ticks[15];
    unsigned int speed[15];
};


bool isValidTrainId(unsigned int);
void initTrainSpeeds();
unsigned int getTrainVelocity(unsigned int, unsigned int);

#endif /* __TRAIN_SPEED_H__ */
