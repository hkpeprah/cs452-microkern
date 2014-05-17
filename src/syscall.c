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


static int sys_create(int priority, void (*code)(), int *retval) {
    task_t * descriptor;
    descriptor = createTaskD(priority);

    if (descriptor != NULL) {
        descriptor->parentTid = currentTask ? currentTask->tid : 0;
        *retval = descriptor->tid;
        return 0;
    }
    return OUT_OF_SPACE;
}


static int sys_tid(bool parent, int *retval) {
    if (currentTask != NULL) {
        switch(parent) {
        case 0:
            *retval = currentTask->tid;
            break;
        case 1:
            *retval = currentTask->parentTid;
            break;
        }
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}


static void sys_pass() {
    task_t *t;
    addTask(currentTask);
    t = schedule();
}


void syscall(unsigned int syscode) {
    int retval;
    int errno;

    switch(syscode) {
    case SYS_EGG:
        errno = 0;
        break;
    case SYS_CREATE:
        errno = sys_create(0, NULL, &retval);
        break;
    case SYS_PASS:
        sys_pass();
        break;
    case SYS_PTID:
        errno = sys_tid(1, &retval);
        break;
    case SYS_MYTID:
        errno = sys_tid(0, &retval);
        break;
    case SYS_EXIT:
        break;
    }
}
