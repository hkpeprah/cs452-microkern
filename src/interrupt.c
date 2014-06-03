/*
 * interrupt.c - interrupt handling, enabling and disabling
 * Notes:
 *    - VIC powers up with vectored interrupts disabled, interupts masked, interrupts giving IRQ
 *    - Write interrupt handler to 0x38
 */
#include <interrupt.h>
#include <term.h>
#include <ts7200.h>

#define INTERRUPT_HANDLER  0x38


extern void irq_handler();
static interruptQueue InterruptTable[NUMBER_INTERRUPTS];


void enableInterrupts() {
    unsigned int i;
    unsigned int *vic;

    for (i = 0; i < NUMBER_INTERRUPTS; ++i) {
        InterruptTable[i].n = 0;
    }

    /* set the handler address */
    *((uint32_t*)INTERRUPT_HANDLER) = (uint32_t)irq_handler;

    /* enable timer */
    vic = (unsigned int*)VIC2_BASE;
    *(vic + VICxIntSelect) = 0;           /* IRQ */
    *(vic + VICxIntEnClear) = 0;          /* clear interrupt bits */
    *(vic + VICxIntEnable) = 1 << 19;     /* enable interrupt */
    debug("Interrupt: Enabling interrupts.");
}


void disableInterrupts() {
    unsigned int *vic;

    vic = (unsigned int*)VIC2_BASE;
    *(vic + VICxIntEnable) = 0;          /* turn off interrupt */
    debug("Interrupt: Disabling interrupts.");
}


void HandleInterrupt() {
    /*
     * Figures out the event corresponding to the mask,
     * and wakes all events in its bucket.
     * Does not return anything.
     */
    puts("YOU CAN'T DOGDGE THE RODGE\r\n");
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
