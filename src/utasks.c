/*
 * utasks.c - User tasks for first part of kernel
 * First User Task
 *    The first user task should create four instances of a test task.
 *        - Two should be at a priority lower than the priority of the first user
 *          task.
 *        - Two should be at a priority higher than the priority of the first user
 *          task.
 *        - The lower priority tasks should be created before the higher priority
 *          tasks.
 *        - On return of each Create, busy-wait IO should be used to print
 *          'Created: <taskid>.' on the RedBoot screen.
 *        - After creating all tasks the first user task should call Exit, immediately
 *          after printing 'First: exiting'.
 * The Other Tasks
 *    The tasks created by the first user task share the same code.
 *        - They first print a line with their task id and their parent’s task id.
 *        - They call Pass.
 *        - They print the same line a second time.
 *        - Finally they call Exit.
 */
#include <term.h>
#include <syscall.h>
#include <utasks.h>
#include <task.h>


void otherTask() {
    char fmt[] = "My Task Id: %d, My Parent's Task ID: %d\n";
    printf(fmt, MyTid(), MyParentTid());
    Pass();
    printf(fmt, MyTid(), MyParentTid());
    Exit();
}


void firstTask() {
    bwprintf(IO, "HELLO WORLD");
    /*
    int i;
    uint32_t tid;
    uint32_t priority;
    task_t *currentTask = getCurrentTask();

    i = -3;
    priority = currentTask->priority;
    while (i < 4) {
        tid = Create(priority + i, &otherTask);
        printf("Created: %d\n", tid);
        i += 2;
    }

    puts("First: exiting\n");
    Exit();
    */
}
