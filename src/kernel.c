#include <task.h>
#include <syscall_types.h>
#include <mem.h>
#include <k_syscall.h>
#include <syscall.h>
#include <term.h>

#define INIT_SPSR          0x13c0
#define SWI_HANDLER_ADDR   0x28
#define FOREVER            while(1)


extern int swi_handler();
extern int swi_exit(int result, int sp, void** tf);
extern int get_cpsr(int dummy);
extern int get_spsr(int dummy);
extern int get_sp(int dummy);

static int handleRequest(k_args_t *args) {
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
        case SYS_PASS:
            sys_pass();
            break;
        case SYS_EXIT:
            sys_exit();
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
    initIO();
    initMem();
    initTasks();
    initSWI();
    clear_screen();
    initDebug();
    puts("Windows ME booting up.");
    newline();
}


void kernel_main() {
    int taskSP;
    k_args_t *args;
    task_t *task = NULL;


    FOREVER {
        task = schedule();

        // nothing left to run
        if (task == NULL) break;
        debugf("Got Task with TID: %d", task->tid);

        // context switch to user task here
        taskSP = swi_exit(task->result, task->sp, (void**) &args);

        // return from swi_exit -> user made a swi call
        task->sp = taskSP;
        task->result = handleRequest(args);
    }
}
