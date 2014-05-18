#include <mem.h>
#include <k_syscall.h>
#include <task.h>
#include <types.h>

#define INIT_SPSR   0x13c0
#define REGS_SAVE   11

int sys_create(int priority, void (*code)(), uint32_t *retval) {
    task_t *task;
    unsigned int i;
    uint32_t originalSP;
    uint32_t *sp;

    task = createTaskD(priority);

    if (task != NULL) {

        /* save the sp, lr and regs 1 - 9 */
        sp = (uint32_t*)task->sp;
//        originalSP = task->sp;

        *(--sp) = (uint32_t)code;
//        *(--sp) = originalSP;
        for (i = 0; i < REGS_SAVE; ++i) {
            *(--sp) = 0;
        }

        *(--sp) = INIT_SPSR;

        task->sp = (uint32_t)sp;
        *retval = task->tid;
        return 0;
    }
    return OUT_OF_SPACE;
}


int sys_tid(uint32_t *retval) {
    task_t *current = getCurrentTask();
    if (current != NULL) {
        *retval = current->tid;
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}

int sys_pid(uint32_t *retval) {
    task_t *current = getCurrentTask();
    if(current != NULL) {
        *retval = current->parentTid;
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}


void sys_pass() {
}


void sys_exit() {
    destroyTaskD();
}
