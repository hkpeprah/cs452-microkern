/*
 * interrupt.c - interrupt handling, enabling and disabling
 * Notes:
 *    - VIC powers up with vectored interrupts disabled, interupts masked, interrupts giving IRQ
 *    - Write interrupt handler to 0x38
 */
#include <interrupt.h>
#include <term.h>
#include <ts7200.h>
#include <clock.h>

#define INTERRUPT_HANDLER    0x38
#define TIMER_INTERRUPT      19
#define EXTRACT_BIT(n, k)    ((n & (1 << k)) >> k)


extern void irq_handler();
static interruptQueue InterruptTable[NUM_INTERRUPTS];


void enableInterrupts() {
    unsigned int i;
    unsigned int *vic;

    for (i = 0; i < NUM_INTERRUPTS; ++i) {
        InterruptTable[i].n = 0;
    }

    /* set the handler address */
    *((uint32_t*)INTERRUPT_HANDLER) = (uint32_t)irq_handler;

    /* enable timer */
    vic = (unsigned int*)VIC2_BASE;
    *(vic + VICxIntSelect) = 0;                        /* IRQ */
    *(vic + VICxIntEnClear) = 0;                       /* clear interrupt bits */
    *(vic + VICxIntEnable) = 1 << TIMER_INTERRUPT;     /* enable interrupt */
    debug("Interrupt: Enabling interrupts.");
}


void disableInterrupts() {
    /* clear interrupts */
    unsigned int *vic;

    *(uint32_t*)TIMER_CONTROL = 0;
    *(uint32_t*)TIMER_CLEAR = 0;

    vic = (unsigned int*)VIC2_BASE;
    vic[VICxIntEnable] = 0;
    debug("Interrupt: Disabling interrupts.");
}


int HandleInterrupt() {
    /*
     * Figures out the event corresponding to the mask,
     * and wakes all events in its bucket.
     * Does not return anything.
     */
    int mask;
    int result;
    unsigned int i;
    unsigned int j;
    unsigned int *vic;
    interruptQueue *table;

    for (i = 0; i < NUM_INTERRUPTS; ++i) {
        table = NULL;
        switch(i) {
            /* determine if status bit set */
            case CLOCK_INTERRUPT:
                vic = (unsigned int*)VIC2_BASE;
                mask = *(vic + VICxIRQStatus);
                if (EXTRACT_BIT(mask, TIMER_INTERRUPT)) {
                    table = &InterruptTable[i];
                    result = 1;
                    *(uint32_t*)TIMER_CLEAR = 0;
                }
                break;
        }

        if (table != NULL) {
            /* wake up all waiting on that queue */
            for (j = 0; j < table->n; ++j) {
                addTask(table->bucket[j]);
                setResult(table->bucket[j], result);
            }
            table->n = 0;
        }
    }

    return 0;
}


int addInterruptListener(int eventType, Task_t *t) {
    /*
     * Adds a task to the bucket awaiting a particular event.
     * Returns 0 on success, -1 on event not existing, -2 on no space
     */
    interruptQueue *table;

    if (eventType < NUM_INTERRUPTS) {
        table = &InterruptTable[eventType];
        if (table->n < MAX_INTERRUPT_LEN) {
            table->bucket[table->n++] = t;
            return 0;
        }
        error("Interrupt: Error: Out of bucket space.");
        return -2;
    }
    error("Interrupt: Error: Invalid interrupt %d given.", eventType);
    return -1;
}
