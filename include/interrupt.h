#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
#include <task.h>

void enableInterrupts();
void disableInterrupts();
int handleInterrupt();
int addInterruptListener(int, Task_t*, void*, int);

#endif /* __INTERRUPT_H__ */
