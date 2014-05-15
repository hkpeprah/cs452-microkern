/*
 * syscall.c - System call functions
 */
#include <mem.h>
#include <syscall.h>
#include <task.h>


int Create(int priority, void (*code)()) {

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
    contextSwitch(currentTask);
}


void Exit() {
    task_t * t;
    /* context switch here */
    contextSwitch(currentTask);
}
