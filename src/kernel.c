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

#define INIT_SPSR          0x13c0
#define SWI_HANDLER_ADDR   0x28
#define FOREVER            while(1)


extern int swi_handler();
extern int swi_exit(int sp, void** tf);
extern int get_cpsr(int dummy);
extern int get_spsr(int dummy);
extern int get_sp(int dummy);


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
        case SYS_AWAIT:
            errno = sys_await(args->a0);
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
    initIO();
    clear_screen();                /* must be before debug */
    initDebug();
    initMem();
    initTasks();
    initSWI();
    seed(43539805);                /* seed random number generator */
    initInterrupts();              /* enable interrupts */
    debug("Successfully booted");
}


void kernel_main() {
    int taskSP;
    int result;
    Args_t *args;
    Task_t *task = NULL;

    FOREVER {
        task = schedule();

        // nothing left to run
        if (task == NULL) break;

        #if DEBUG_KERNEL
            int * sp;
            int i;
            debugf("Kernel: switching to task with tid: %d,\r\nStack:", task->tid);
            *sp = (int*) task->sp;
            for (i = 0; i < 16; ++i) {
                debugf("0x%x: 0x%x", sp + i, sp[i]);
            }
        #endif

        // context switch to user task here
        taskSP = swi_exit(task->sp, UNION_CAST(&args, void**));

        // return from swi_exit -> user made a swi call
        task->sp = taskSP;
        result = handleRequest(args);
        setResult(task, result);

        #if DEBUG_KERNEL
            sp = (int*) task->sp;
            debugf("Kernel: Got request for task %d with result %d", task->tid, result);
            for (i = 0; i < 16; ++i) {
                debugf("0x%x: 0x%x", sp + i, sp[i]);
            }
        #endif
    }
}
