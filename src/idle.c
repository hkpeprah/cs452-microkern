#include <idle.h>
#include <types.h>
#include <clock.h>
#include <term.h>
#include <stdlib.h>

#define TIMER4VALUELOW    0x80810060
#define TIMER4VALUEHIGH   0x80810064
#define TIMER4ENABLE      TIMER4VALUEHIGH
#define TIMER4INTERVAL    50

static volatile int count;
static volatile int lastCount;
static volatile int lastIdle;
static volatile int totalIdle;
static volatile uint32_t *timerHigh;
static volatile uint32_t *timerLow;


void cpu_idle(bool isIdle) {
    static int t = 0;

    if ((*timerLow / CYCLES_PER_TICK) - count >= 1) {
        count = (*timerLow / CYCLES_PER_TICK);
        if ((count - lastCount) / TIMER4INTERVAL > 0) {
            lastCount = ABS(count - lastCount);
            lastIdle = ABS(totalIdle - lastIdle);
        }
    }

    if (isIdle == true) {
        /* when state is idle, set t to the current time */
        t = count;
    } else if (t > 0 && isIdle == false) {
        /* when state is no longer idle, add the difference */
        if (lastCount != count) {
            totalIdle += (count - t);
        }
        t = 0;
    }
}


int getIdleTime() {
    return (lastIdle * 100) / lastCount;
}


void enableIdleTimer() {
    timerHigh = (uint32_t*)TIMER4VALUEHIGH;
    timerLow = (uint32_t*)TIMER4VALUELOW;
    *((uint16_t*)TIMER4ENABLE) = 0;
    NOOP_PTR(timerLow);
    NOOP_PTR(timerHigh);
    *((uint16_t*)TIMER4ENABLE) = 0x100;
    do {
        /* force clear of an overflow, this is a hacky solution
           to the situation where users are not leaving Timer4
           in a reset state when they exit, but instead are
           overflowing it */
        NOOP_PTR(timerLow);
        NOOP_PTR(timerHigh);
    } while ((int32_t)(*timerLow) <= 0);

    /* set initial values for the global counters */
    totalIdle = 1;
    lastIdle = 1;
    lastCount = 1;
    count = 1;
}


void disableIdleTimer() {
    *timerHigh = 0x000;
    /* clears the 40-bit timer by issuing a read */
    (void)*timerLow;
    (void)*timerHigh;
}
