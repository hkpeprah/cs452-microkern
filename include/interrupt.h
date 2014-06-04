#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
#include <task.h>


typedef enum {
    CLOCK_INTERRUPT = 0,
    NUM_INTERRUPTS
} interrupt_types;


void enableInterrupts();
void disableInterrupts();
int handleInterrupt();
int addInterruptListener(int, Task_t*);

#endif /* __INTERRUPT_H__ */
