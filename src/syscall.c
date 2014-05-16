/*
 * syscall.c - System call functions
 */
#include <mem.h>
#include <syscall.h>
#include <task.h>


int Create(int priority, void (*code)()) {
    task_t * descriptor;

    descriptor = createTaskD(priority);
    descriptor->parentTid = currentTask ? currentTask->tid : 0;

    return descriptor->tid;
}


int MyTid() {
    return currentTask->tid;
}


int myParentTid() {
    return currentTask->parentTid;
}


void Pass() {
    task_t * t;
    /* context switch here */
}


void Exit() {
    task_t * t;
    t = currentTask;
    destroyTaskD(t);
    /* context switch to kernel */
}
