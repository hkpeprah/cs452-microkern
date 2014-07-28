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
#include <stdlib.h>
#include <logger.h>

#define INTERRUPT_HANDLER    0x38
#define TIMER_INTERRUPT      (TC3OI - 32)
#define UART1_INTERRUPT      (INT_UART1 - 32)
#define UART2_INTERRUPT      (INT_UART2 - 32)

typedef struct {
    Task_t *blockedTask;
    void *buf;
    int buflen;
} AwaitTask_t;


extern void irq_handler();
static AwaitTask_t interruptTable[NUM_INTERRUPTS];

static volatile int *u1int = (int*) (UART1_BASE + UART_INTR_OFFSET);
static volatile int *u1flag = (int*) (UART1_BASE + UART_FLAG_OFFSET);
static volatile int *u1data = (int*) (UART1_BASE + UART_DATA_OFFSET);
static volatile int *u1ctlr = (int*) (UART1_BASE + UART_CTLR_OFFSET);

static volatile int *u2int = (int*) (UART2_BASE + UART_INTR_OFFSET);
static volatile int *u2flag = (int*) (UART2_BASE + UART_FLAG_OFFSET);
static volatile int *u2data = (int*) (UART2_BASE + UART_DATA_OFFSET);
static volatile int *u2ctlr = (int*) (UART2_BASE + UART_CTLR_OFFSET);

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
    kdebug("Interrupt: Enabling interrupts.");
}


void disableInterrupts() {
    /* clear interrupts */
    unsigned int *vic;

    *(uint32_t*)TIMER_CONTROL = 0;
    *(uint32_t*)TIMER_CLEAR = 0;

    *u1ctlr = 1;
    *u2ctlr = 1;

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
    uint32_t vic1, vic2;
    int result = 0;
    char *buf;
    int buflen;
    InterruptType_t type = NUM_INTERRUPTS;
    Task_t *task = NULL;

    // TODO: refactor code using these ptrs
    vic1base = (int*) VIC1_BASE;
    vic2base = (int*) VIC2_BASE;

    vic1 = vic1base[VICxIRQStatus];
    vic2 = vic2base[VICxIRQStatus];

    // Stop complaints about unused variable
    (void)u1flag;

    if (EXTRACT_BIT(vic2, TIMER_INTERRUPT)) {
        type = CLOCK_INTERRUPT;
        task = interruptTable[CLOCK_INTERRUPT].blockedTask;
        if (!task) {
            kerror("FATAL: missed clock tick");
            return -1;
        }
        *((uint32_t*)TIMER_CLEAR) = 0;
        result = 1;
    } else if (EXTRACT_BIT(vic2, UART1_INTERRUPT)) {
        if (*u1int & RIS_MASK) {
            type = UART1_RCV_INTERRUPT;
            task = interruptTable[type].blockedTask;

            if (task) {
                result = *u1data & DATA_MASK;
            } else {
                // TODO: no waiting task, should send stop control bit
                sys_log_f("Uart1 rcv interrupt missed\n");
                *u1ctlr &= ~(RIEN_MASK);
            }
        } else if (*u1int & TIS_MASK) {
            type = UART1_XMT_INTERRUPT;
            *u1ctlr &= ~(TIEN_MASK);
            task = interruptTable[type].blockedTask;
            if (!task) {
                sys_log_f("uart1 xmt interrupt missed\n");
            }
            result = 1;
        } else if (*u1int & MIS_MASK) {
            type = UART1_MOD_INTERRUPT;
            task = interruptTable[type].blockedTask;
            result = 1;

            if (task) {
                *u1int = 0;
            } else {
                // TODO: no waiting task, mask intr
                sys_log_f("Uart1 MOD interrupt missed\n");
                *u1ctlr &= ~(MSIEN_MASK);
            }
        }

    } else if (EXTRACT_BIT(vic2, UART2_INTERRUPT)) {

        if (*u2int & (RIS_MASK | RTIS_MASK)) {
            type = UART2_RCV_INTERRUPT;
            task = interruptTable[type].blockedTask;

            if (task) {
                buf = interruptTable[type].buf;
                buflen = interruptTable[type].buflen;
                result = 0;

                while (!(*(u2flag) & RXFE_MASK) && buflen --> 0) {
                    buf[result++] = *u2data & DATA_MASK;
                }
            } else {
                // TODO: no waiting task -> input is faster than we are reading
                // should mask interrupt, send stop bit to sender
                sys_log_f("uart2 rcv interrupt missed\n");
                *u2ctlr &= ~(RIEN_MASK);
            }



        } else if (*u2int & TIS_MASK) {
            type = UART2_XMT_INTERRUPT;
            // disable xmit interrupt
            *u2ctlr &= ~(TIEN_MASK);
            task = interruptTable[type].blockedTask;
            if (!task) {
                sys_log_f("uart2 xmt interrupt missed\n");
            }
            result = 1;
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
        kdebug("Interrupt: Error: Invalid interrupt %d given.", eventType);
        return -1;
    }

    taskEntry = &interruptTable[eventType]; 

    if (taskEntry->blockedTask) {
        kdebug("Interrupt: Error: Task already waiting (tid=%d).", taskEntry->blockedTask->tid);
        return -2;
    }

    // something waiting on XMT, so enable those interrupts
    switch (eventType) {
        case UART1_RCV_INTERRUPT:
            *u1ctlr |= RIEN_MASK;
            break;
        case UART1_XMT_INTERRUPT:
            *u1ctlr |= TIEN_MASK;
            break;
        case UART1_MOD_INTERRUPT:
            *u1ctlr |= MSIEN_MASK;
            break;
        case UART2_RCV_INTERRUPT:
            *u2ctlr |= RIEN_MASK | RTIEN_MASK;
            break;
        case UART2_XMT_INTERRUPT:
            *u2ctlr |= TIEN_MASK;
            break;
    }

    taskEntry->blockedTask = t;
    taskEntry->buf = buf;
    taskEntry->buflen = buflen;

    return 0;
}
