#include <idle.h>
#include <types.h>
#include <clock.h>
#include <term.h>

static volatile int idle = 0;
static volatile int count = 0;
static volatile uint32_t *timerHigh = (uint32_t*)0x80810064;
static volatile uint32_t *timerLow = (uint32_t*)0x80810060;


void cpu_idle(bool isIdle) {
    static int t = 0;

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

    *timerHigh = 0x100;
    idle = 0;
    count = 1;
    /* clear the lower of the 40 bit timer */
    (void)*timerLow;

    /* force clear of an overflow */
    do {
        timerValue = (int32_t)(*timerLow);
    } while (timerValue < 0);
}


void disableIdleTimer() {
    *timerHigh = 0x000;
    /* clears the 40-bit timer by issuing a read */
    (void)*timerLow;
    (void)*timerHigh;
}
