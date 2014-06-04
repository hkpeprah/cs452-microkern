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
static Task_t *interruptTable[NUM_INTERRUPTS];


void enableInterrupts() {
    unsigned int i;
    unsigned int *vic;

    for (i = 0; i < NUM_INTERRUPTS; ++i) {
        interruptTable[i] = NULL;
    }

    /* set the handler address */
    *((uint32_t*)INTERRUPT_HANDLER) = (uint32_t)irq_handler;

    /* enable timer */
    vic = (unsigned int*)VIC2_BASE;
    vic[VICxIntSelect] = 0;                        /* IRQ */
    vic[VICxIntEnClear] = 0;                       /* clear interrupts */
    vic[VICxIntEnable] = 1 << TIMER_INTERRUPT;     /* enable interrupt */
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

int handleInterrupt() {
    /*
     * Figures out the event corresponding to the mask,
     * and wakes all events in its bucket.
     * Does not return anything.
     */

    uint32_t vic1, vic2;
    int result = 0;
    Task_t **taskp = NULL;

    vic1 = ((uint32_t*)VIC1_BASE)[VICxIRQStatus];
    vic2 = ((uint32_t*)VIC2_BASE)[VICxIRQStatus];

    if (EXTRACT_BIT(vic2, TIMER_INTERRUPT)) {
        taskp = &interruptTable[CLOCK_INTERRUPT];
        *((uint32_t*)TIMER_CLEAR) = 0;
        result = 1;
    }

    if (*taskp) {
        addTask(*taskp);
        setResult(*taskp, result);
    }

    return 0;
}


int addInterruptListener(int eventType, Task_t *t) {
    /*
     * Adds a task to the bucket awaiting a particular event.
     * Returns 0 on success, -1 on event not existing, -2 on no space
     */
    Task_t **taskp;

    if (eventType >= NUM_INTERRUPTS) {
        error("Interrupt: Error: Invalid interrupt %d given.", eventType);
        return -1;
    }

    taskp = &interruptTable[eventType];

    if (*taskp) {
        error("Interrupt: Error: Task already waiting.");
        return -2;
    }

    *taskp = t;
    return 0;
}
