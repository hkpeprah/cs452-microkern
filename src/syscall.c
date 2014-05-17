/*
 * syscall.c - System call functions
 */
#include <mem.h>
#include <syscall.h>
#include <task.h>
#include <types.h>


int MyTid() {
    return 0;
}


int MyParentTid() {
    return 0;
}


int Create(int priority, void (*code) ()) {
    return 0;
}


void Pass() {
}


void Exit() {
}


int sys_create(int priority, void (*code)(), uint32_t *retval) {
    task_t *descriptor;
    unsigned int i;
    task_t *current = getCurrentTask();
    descriptor = createTaskD(priority);

    if (descriptor != NULL) {
        descriptor->parentTid = current ? current->tid : 0;
        *(uint32_t*)descriptor->sp-- = (uint32_t)code;
        for (i = 0; i < REGS_SAVE; ++i) {
            *(uint32_t*)descriptor->sp-- = 0;
        }
        *retval = descriptor->tid;
        return 0;
    }
    return OUT_OF_SPACE;
}


int sys_tid(bool parent, uint32_t *retval) {
    task_t *current = getCurrentTask();
    if (current != NULL) {
        switch(parent) {
        case 0:
            *retval = current->tid;
            break;
        case 1:
            *retval = current->parentTid;
            break;
        }
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}


void sys_pass() {
    task_t *t;
    t = schedule();
}


void sys_exit() {
    task_t *t = getCurrentTask();
    destroyTaskD(t);
    t = NULL;
}


void syscall(unsigned int syscode, void *tf) {
    int errno;

    switch(syscode) {
    case SYS_EGG:
        errno = 0;
        break;
    case SYS_CREATE:
        break;
    case SYS_PASS:
        sys_pass();
        break;
    case SYS_PTID:
        break;
    case SYS_MYTID:
        break;
    case SYS_EXIT:
        break;
    }
}
