#ifndef __CLOCK_H__
#define __CLOCK_H__
#include <types.h>

#define TIMER_LOAD     0x80810080
#define TIMER_VALUE    0x80810084
#define TIMER_CONTROL  0x80810088
#define TIMER_CLEAR    0x8081008C
#define TIMER_ENABLE   0x00000080
#define TIMER_MODE     0x00000040
#define TIMER_508KHZ   0x00000008

#define CLOCK_SERVER   "ClockServer"


struct DelayQueue_t;


typedef enum {
    DELAY = 0,
    DELAY_UNTIL,
    TIME,
    TICK
} ClockRequestType;


typedef struct {
    short type;
    unsigned int ticks;
} ClockRequest;


typedef struct DelayQueue_t {
    uint32_t tid : 8;
    uint32_t delay;
    struct DelayQueue_t *next;
} DelayQueue;


void ClockServer();
int Time();
int Delay(int);
int DelayUntil(int);

#endif /* __CLOCK_H__ */
