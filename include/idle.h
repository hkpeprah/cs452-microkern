#ifndef __IDLE_H__
#define __IDLE_H__
#include <types.h>
#define HZ                100              /* 100 ticks in a hertz */
#define LOAD_FREQ         5 * HZ           /* calculated over 500 ticks */
#define CYCLES_PER_TICK   9380             /* number of clock cycles per tick of the 40-bit timer */
#define MHZ_TO_HZ(x)      (x * 1000000)    /* one million hertz in a megahertz */


void cpu_idle(bool isIdle);
int getIdleTime();
void enableIdleTimer();
void disableIdleTimer();

#endif /* __IDLE_H__ */
