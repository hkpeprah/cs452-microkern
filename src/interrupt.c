#include <interrupt.h>
#include <term.h>


static interruptQueue InterruptTable[NUMBER_INTERRUPTS];


void initInterrupts() {
    unsigned int i;
    for (i = 0; i < NUMBER_INTERRUPTS; ++i) {
        InterruptTable[i].n = 0;
    }
}


void HandleInterrupt(int mask) {
    /*
     * Figures out the event corresponding to the mask,
     * and wakes all events in its bucket.
     * Does not return anything.
     */
}


int addInterruptListener(int eventType, Task_t *t) {
    /*
     * Adds a task to the bucket awaiting a particular event.
     * Returns 0 on success, -1 on event not existing, -2 on no space
     */
    interruptQueue table;

    if (eventType < NUMBER_INTERRUPTS) {
        table = InterruptTable[eventType];
        if (table.n < MAX_INTERRUPT_LEN) {
            debugf("Interrupt: Task %d waiting on %d", t->tid, eventType);
            table.bucket[table.n++] = t;
            return 0;
        }
        return -2;
    }
    return -1;
}
