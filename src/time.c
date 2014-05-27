/*
 * time.c - hardwware time / timing functions
 */
#include <time.h>


static Timer Clock;


void resetClock(Timer t, uint32_t speed) {
    /*
     * resets the specified clock.
     */
    timer.t_seconds = 0;
    timer.seconds = 0;
    timer.minutes = 0;
    timer.hours = 0;
    timer.count = 0;
    timer.speed = speed;
    timer.clk = (uint32_t*)TIMER_VALUE;
    timer.base = (uint32_t*)TIMER_BASE;
    timer.control = (uint32_t*)TIMER_CONTROL;

    *timer.base = 0xFFFFFFFF;
    switch(speed) {
    case 50800:
        *timer.control = ENABLE_CLOCK | 0x00000008;
    default:
        *timer.control = ENABLE_CLOCK;
    }

    *timer.base = 0xFFFFFFFF;
    timer.count = 0xFFFFFFFF;
}


bool tick(Timer t) {
    /*
     * updates the time
     * returns 0 if the time changed, otherwise 1
     */
    if (t.count - *(t.clk) >= t.speed) {
        t.count = *(t.clk);
        if (++t.t_seconds == 10) {
            t.t_seconds = 0;
            if (++t.seconds == 60) {
                t.seconds = 0;
                ++t_minutes;
            }
        }
        return 1;
    }

    return 0;
}


double currentTime(Timer t) {
    return t.seconds + (t.minutes * 60) + (t.t_seconds / 10);
}


uint32_t getTimerValue(Timer t) {
    return *(t.clk);
}
