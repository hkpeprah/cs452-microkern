#ifndef __CLOCK_H__
#define __CLOCK_H__
#include <types.h>

#define TIMER_CONTROL  0x80810088
#define TIMER_CLEAR    0x8081008C

void ClockServer();
int Time();
int Delay(int ticks);
int DelayUntil(int ticks);

// delay on a courier
// returns courier's TID for the creator to wait on, and when the
// courier sends to the creator the delay will have finished
int CourierDelay(int ticks, int priority);

#endif /* __CLOCK_H__ */
