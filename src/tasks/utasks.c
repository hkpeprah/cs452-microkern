/*
 * utasks.c - User tasks for second part of kernel
 * First Task
 *    - Bootstraps the other tasks and the clients.
 *    - Quits when it has gone through all the clients.
 */
#include <term.h>
#include <utasks.h>
#include <task.h>


void testTask() {
    printf("Calling task with priority: %d\r\n", getCurrentTask()->priority);
    Exit();
}


void firstTask() {
    unsigned int tid, i, priority;
    /* do nothing for now */
    Pass();
    Exit();
}
