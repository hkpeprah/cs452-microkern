#include <utasks.h>
#include <task.h>
#include <mem.h>
#include <term.h>
#include <stdio.h>
#include <syscall.h>
#include <k_syscall.h>
#include <syscall_types.h>

#define INIT_SPSR          0x13c0
#define SWI_HANDLER_ADDR   0x28
#define FOREVER            while(1)
#define FIRST_PRIORITY     5

extern int swi_handler();
extern int swi_exit(int result, int sp, void** tf);
extern int get_cpsr(int dummy);
extern int get_spsr(int dummy);
extern int get_sp(int dummy);


static void initSWI() {
    uint32_t *swiHandlerAddr = (uint32_t*)SWI_HANDLER_ADDR;
    *swiHandlerAddr = (uint32_t)swi_handler;
}

static void boot() {
    initIO();
    initMem();
    initTasks();
    initSWI();
    clear_screen();
    initDebug();
    puts("Windows ME booting up.");
    newline();
}

static int handleRequest(k_args_t *args) {
    uint32_t result = 0;
    uint32_t errno = 0;

    switch(args->code) {
        case SYS_EGG: // wat?
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

    if(errno == 0) {
        return result;
    }

    return errno;
}

static void kernel_main() {
    task_t *task = NULL;
    int taskSP;
    k_args_t *args;

    for(;;) {
        task = schedule();

        if(task == NULL) {
            break;
        }

        // context switch to user task here
        taskSP = swi_exit(task->result, task->sp, (void**) &args);

        // return from swi_exit -> user made a swi call
        task->sp = taskSP;
        task->result = handleRequest(args);
    }
}


int main() {
    int status;
    uint32_t tid;

    boot();

    /* create the first user task */
    status = sys_create(FIRST_PRIORITY, firstTask, &tid);
    if (status != 0) {
        /* something went wrong creating the first user task */
        return -1;
    }
    
    kernel_main();

    // should never reach here!
    puts("Exiting...\n");
    return 0;
}
