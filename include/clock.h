#ifndef __CLOCK_H__
#define __CLOCK_H__

#define TIMER_BASE     0x80810080
#define TIMER_VALUE    0x80810084
#define TIMER_CONTROL  0x80810088
#define TIMER_CLEAR    0x8081008C

#define ENABLE_CLOCK   0x00000080

#define CLOCK_SERVER   "ClockServer"


typedef enum {
    DELAY = 0,
    TIME,
    DELAY_UNTIL
} ClockRequestTypes;


typedef struct {
    short type;
    int ticks;
} ClockRequest;


void ClockServer();
int Time();
int Delay(int);
int DelayUntil(int);

#endif /* __CLOCK_H__ */
