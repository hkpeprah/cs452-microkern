#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
#include <task.h>

#define MAX_INTERRUPT_LEN 8


typedef enum {
    CLOCK_INTERRUPT = 0,
    NUM_INTERRUPTS
} interrupt_types;


typedef struct {
    unsigned int n;
    Task_t *bucket[MAX_INTERRUPT_LEN];
} interruptQueue;


void enableInterrupts();
void disableInterrupts();
int HandleInterrupt();
int addInterruptListener(int, Task_t*);

#endif /* __INTERRUPT_H__ */
