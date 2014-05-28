#ifndef __TIME_H__
#define __TIME_H__
#include <stdlib.h>

#define TIMER_BASE     0x80810080
#define TIMER_VALUE    0x80810084
#define TIMER_CONTROL  0x80810088
#define TIMER_CLEAR    0x8081008C

#define ENABLE_CLOCK   0x00000080


typedef struct {
    uint32_t t_seconds;
    uint32_t seconds;
    uint32_t minutes;
    uint32_t hours;
    uint32_t count;
    uint32_t speed;
    uint32_t *clk;
    uint32_t *base;
    uint32_t *control;
} Timer;


void initClock();
void resetClock(Timer, uint32_t);
bool tick(Timer);
double currentTime();
uint32_t getTimerValue();

#endif /* __TIME_H__ */
