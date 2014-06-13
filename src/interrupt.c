/*
 * interrupt.c - interrupt handling, enabling and disabling
 * Notes:
 *    - VIC powers up with vectored interrupts disabled, interupts masked, interrupts giving IRQ
 *    - Write interrupt handler to 0x38
 */
#include <interrupt.h>
#include <types.h>
#include <term.h>
#include <ts7200.h>
#include <clock.h>

#define INTERRUPT_HANDLER    0x38
#define TIMER_INTERRUPT      (TC3OI - 32)
#define UART1_INTERRUPT      (INT_UART1 - 32)
#define UART2_INTERRUPT      (INT_UART2 - 32)
#define EXTRACT_BIT(n, k)    ((n & (1 << k)) >> k)

typedef struct {
    Task_t *blockedTask;
    void *buf;
    int buflen;
} AwaitTask_t;


extern void irq_handler();
static AwaitTask_t interruptTable[NUM_INTERRUPTS];

static void clearEntry(InterruptType_t i) {
    interruptTable[i].blockedTask = NULL;
    interruptTable[i].buf = NULL;
    interruptTable[i].buflen = 0;
}


void enableInterrupts() {
    unsigned int i;
    unsigned int *vic;

    for (i = 0; i < NUM_INTERRUPTS; ++i) {
        interruptTable[i].blockedTask = NULL;
        interruptTable[i].buf = NULL;
        interruptTable[i].buflen = 0;
    }

    /* set the handler address */
    *((uint32_t*)INTERRUPT_HANDLER) = (uint32_t)irq_handler;

    /* enable timer */
    vic = (unsigned int*)VIC2_BASE;
    vic[VICxIntSelect] = 0;                        /* IRQ */
    vic[VICxIntEnClear] = 0;                       /* clear interrupts */
    vic[VICxIntEnable] = (1 << TIMER_INTERRUPT) | (1 << UART2_INTERRUPT) | (1 << UART1_INTERRUPT);
    //debug("Interrupt: Enabling interrupts.");
}


void disableInterrupts() {
    /* clear interrupts */
    unsigned int *vic;

    *(uint32_t*)TIMER_CONTROL = 0;
    *(uint32_t*)TIMER_CLEAR = 0;

    vic = (unsigned int*)VIC2_BASE;
    vic[VICxIntEnable] = 0;
}


int handleInterrupt() {
    /*
     * Figures out the event corresponding to the mask,
     * and wakes all events in its bucket.
     * Does not return anything.
     */

    int *vic1base, *vic2base;
    uint32_t vic1, vic2, status;
    int result = 0;
    int intr = 0;
    InterruptType_t type;
    Task_t *task = NULL;

    // TODO: refactor code using these ptrs
    vic1base = (int*) VIC1_BASE;
    vic2base = (int*) VIC2_BASE;

    vic1 = ((uint32_t*)VIC1_BASE)[VICxIRQStatus];
    vic2 = ((uint32_t*)VIC2_BASE)[VICxIRQStatus];

//    printf("vic2: 0x%x\n", vic2);

    if (EXTRACT_BIT(vic2, TIMER_INTERRUPT)) {
        type = CLOCK_INTERRUPT;
        task = interruptTable[CLOCK_INTERRUPT].blockedTask;
        *((uint32_t*)TIMER_CLEAR) = 0;
        result = 1;
    } else if (EXTRACT_BIT(vic2, UART1_INTERRUPT)) {
        status = *((int*) (UART1_BASE + UART_FLAG_OFFSET));
        printf("uart 1 flag: 0x%x\n", status & 0xFF);
        status = *((int*) (UART1_BASE + UART_INTR_OFFSET));
        printf("uart 1 intr: 0x%x\n", status & 0xF);

    } else if (EXTRACT_BIT(vic2, UART2_INTERRUPT)) {
        // which interrupt is triggered?
        intr = *((int*) (UART2_BASE + UART_INTR_OFFSET));

        if (intr & RIS_MASK) {
            result = *((int*) (UART2_BASE + UART_DATA_OFFSET)) & DATA_MASK;
            task = interruptTable[UART2_RCV_INTERRUPT].blockedTask;
        } else if (intr & TIS_MASK) {
            // disable xmit interrupt
            *((int*) (UART2_BASE + UART_CTLR_OFFSET)) &= ~(TIEN_MASK);
            task = interruptTable[UART2_XMT_INTERRUPT].blockedTask;
            result = 1;
        } else if (intr & MIS_MASK) {

        } else if (intr & RTIS_MASK) {

        }
    }

    if (task) {
        addTask(task);
        setResult(task, result);
        clearEntry(type);
    }

    return 0;
}


int addInterruptListener(int eventType, Task_t *t, void *buf, int buflen) {
    /*
     * Adds a task to the bucket awaiting a particular event.
     * Returns 0 on success, -1 on event not existing, -2 on no space
     */

    AwaitTask_t *taskEntry = NULL;

    if (eventType >= NUM_INTERRUPTS) {
        error("Interrupt: Error: Invalid interrupt %d given.", eventType);
        return -1;
    }

    taskEntry = &interruptTable[eventType]; 

    if (taskEntry->blockedTask) {
        error("Interrupt: Error: Task already waiting.");
        return -2;
    }

    // something waiting on XMT, so enable those interrupts
    if (eventType == UART1_XMT_INTERRUPT) {
        *((int*) (UART1_BASE + UART_CTLR_OFFSET)) |= TIEN_MASK;
    } else if (eventType == UART2_XMT_INTERRUPT) {
        *((int*) (UART2_BASE + UART_CTLR_OFFSET)) |= TIEN_MASK;
    }

    taskEntry->blockedTask = t;
    taskEntry->buf = buf;
    taskEntry->buflen = buflen;

    return 0;
}
