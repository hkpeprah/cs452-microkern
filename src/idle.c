#include <idle.h>
#include <types.h>
#include <clock.h>
#include <term.h>

static volatile unsigned int idle = 0;
static volatile unsigned int count = 1;


void cpuIdle(bool isIdle) {
    static bool enabled = false;
    static bool wasIdle = 0;
    volatile uint32_t *timerHigh = (uint32_t*)0x80810064;
    volatile uint32_t *timerLow = (uint32_t*)0x80810060;
    int ticks = 0;

    if (enabled == false) {
        *timerHigh = 0x100;
        enabled = true;
        idle = 0;
        count = 0;
    } else {
        if ((*timerLow / 9380) - count >= 1) {
            ticks = (*timerLow / 9380) - count;
            count += ticks;
        }

        if (wasIdle > 0 && !isIdle) {
            idle += (count - wasIdle);
            wasIdle = 0;
        }

        if (isIdle && wasIdle == 0) {
            wasIdle = count;
        }

        if (idle > count) {
            /*
             * TODO: Figure out why we need this, some race conditioning
             */
            idle = 0;
        }
    }
}


int getIdleTime() {
    return ((double)idle / count) * 100;
}
