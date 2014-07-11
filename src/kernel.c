#include <task.h>
#include <syscall_types.h>
#include <mem.h>
#include <k_syscall.h>
#include <syscall.h>
#include <term.h>
#include <clock.h>
#include <random.h>
#include <stdlib.h>
#include <interrupt.h>
#include <train.h>
#include <uart.h>
#include <logger.h>
#include <shell.h>
#include <idle.h>
#include <null.h>
#include <train_speed.h>

#define SWI_HANDLER_ADDR   0x28
#define FOREVER            while(1)


extern int swi_handler();
extern int swi_exit(int sp, void** tf);

static void cacheOn() {
    /*
     * cache bits 0 (mmu), 2 (dcache), 12 (icache)
     * enable cache and invalidate it
     */
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "orr r0, r0, #4096\n\t"
        "orr r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "mcr p15, 0, r0, c7, c7, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}

#if 0
/* this seems to break stuff. Don't use it. */
static void cacheOff() {
    /* clear cache enable bits */
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "bic r0, r0, #4096\n\t"
        "bic r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}
#endif


static int handleRequest(Args_t *args) {
    uint32_t result = 0;
    uint32_t errno = 0;

    switch (args->code) {
        case SYS_EGG: // easter egg
            errno = 0;
            break;
        case SYS_CREATE:
            errno = sys_create(args->a0, (void*)args->a1, &result);
            break;
        case SYS_MYTID:
            errno = sys_tid(&result);
            break;
        case SYS_PTID:
            errno = sys_pid(&result);
            break;
        case SYS_SEND:
            errno = sys_send(args->a0, (void*)args->a1, args->a2, (void*)args->a3, args->a4);
            break;
        case SYS_RECV:
            errno = sys_recv((int*)args->a0, (void*)args->a1, args->a2);
            break;
        case SYS_REPL:
            errno = sys_reply(args->a0, (void*)args->a1, args->a2);
            break;
        case SYS_PASS:
            sys_pass();
            break;
        case SYS_EXIT:
            sys_exit();
            break;
        case SYS_DESTROY:
            errno = sys_destroy(args->a0);
            break;
        case SYS_AWAIT:
            errno = sys_await(args->a0, (void *)args->a1, args->a2);
            break;
        case SYS_INTERRUPT:
            errno = handleInterrupt();
            break;
        case SYS_WAITTID:
            errno = sys_waittid(args->a0);
            break;
        case SYS_LOG:
            errno = sys_log((const char *)args->a0, args->a1);
            break;
        case SYS_IDLE:
            sys_idle(&result);
            break;
        case SYS_SIGTERM:
            sys_sigterm();
            break;
        default:
            break;
    }

    if (errno == 0) {
        return result;
    }

    return errno;
}


static void initSWI() {
    uint32_t *swiHandlerAddr = (uint32_t*)SWI_HANDLER_ADDR;
    *swiHandlerAddr = (uint32_t)swi_handler;
}


void boot () {
    /* sequence of boot operations */
    cacheOn();
    initUart(IO, 115200, true);
    initUart(TRAIN, 2400, false);
    initDebug();
    initMem();
    initTasks();
    initSWI();
    initLogger();
    initTrainSpeeds();
    initTrainSet();
    turnOnTrainSet();
    displayInfo();
    seed(43539805);                /* seed random number generator */
    enableInterrupts();            /* enable interrupts */
    kdebug("Kernel: Booted");
}


int shutdown() {
    /* sequence of shutdown operations */
    kputstr(RESTORE_CURSOR);
    kputstr("\r\nShutting down the system...\r\n");
    kputstr("Disabling interrupts..\r\n");
    disableInterrupts();
    kputstr("Turning off the train controller...\r\n");
    disableIdleTimer();
    *((uint32_t*)TIMER_CONTROL) = 0;
    kputstr(SAVE_CURSOR "\033[0;0r" RESTORE_CURSOR "\r\nExiting...\r\n");

    return 0;
}


void kernel_main() {
    int taskSP;
    int result;
    int status;
    uint32_t nullTaskTid;
    Args_t *args;
    Args_t defaultArg = {.code = SYS_INTERRUPT};
    Task_t *task = NULL;

    status = sys_create(0, NullTask, &nullTaskTid);
    setExit(0);
    cpuIdle(false);

    FOREVER {
        task = schedule();

        if (task->tid != 1 && task->tid != 3 && task->tid != 4) {
            sys_log_f("%d\n", tid);
        }

        if (task == NULL) {
            // nothing left to run
            break;
        } else if (task->tid == nullTaskTid) {
            // switching to the null task, cpu is idle
            cpuIdle(true);
        } else {
            cpuIdle(false);
        }

        // default to interrupt, ie. if not swi then the pointer is not modified and we know
        // it is an interrupt that needs to be handled
        args = &defaultArg;

        // context switch to user task here
        taskSP = swi_exit(task->sp, UNION_CAST(&args, void**));

        // store user stack pointer
        task->sp = taskSP;

        if (args->code == SYS_SIGTERM) {
            break;
        }

        if (task->tid != 1 && task->tid != 3 && task->tid != 4) {
            sys_log_f("req %d\n", args->code);
        }

        result = handleRequest(args);

        if (task->tid != 1 && task->tid != 3 && task->tid != 4) {
            sys_log_f("hndl %d\n", args->code);
        }

        if (args->code != SYS_INTERRUPT && args->code != SYS_EXIT) {
            setResult(task, result);
        }
    }

    dumpTaskState();
}
