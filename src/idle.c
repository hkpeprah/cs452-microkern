#include <idle.h>
#include <types.h>
#include <clock.h>
#include <term.h>
#include <stdlib.h>

#define TIMER4VALUELOW    0x80810060
#define TIMER4VALUEHIGH   0x80810064
#define TIMER4ENABLE      TIMER4VALUEHIGH

static volatile unsigned int idle;
static volatile unsigned int count;
static volatile uint32_t *timerHigh;
static volatile uint32_t *timerLow;


void cpu_idle(bool isIdle) {
    static unsigned int t = 0;

    if ((*timerLow / CYCLES_PER_TICK) - count >= 1) {
        count = (*timerLow / CYCLES_PER_TICK);
    }

    if (isIdle == true) {
        /* when state is idle, set t to the current time */
        t = count;
    } else if (t > 0 && isIdle == false) {
        /* when state is no longer idle, add the difference */
        idle += (count - t);
        t = 0;
    }
}


int getIdleTime() {
    return (idle * 100) / count;
}


void enableIdleTimer() {
    int timerValue;
    timerHigh = (uint32_t*)TIMER4VALUEHIGH;
    timerLow = (uint32_t*)TIMER4VALUELOW;
    *((int32_t*)TIMER4ENABLE) = 0x100; /* set the enable bit (8th bit) to 1 */
    do {
        /* force clear of an overflow, this is a hacky solution
           to the situation where users are not leaving Timer4
           in a reset state when they exit, but instead are
           overflowing it */
        NOOP_PTR(timerLow);
        NOOP_PTR(timerHigh);
        timerValue = (int32_t)(*timerLow);
    } while (timerValue <= 0);

    /* set initial values for the global counters */
    idle = *timerLow;
    count = *timerLow;
}


void disableIdleTimer() {
    *timerHigh = 0x000;
    /* clears the 40-bit timer by issuing a read */
    (void)*timerLow;
    (void)*timerHigh;
}
