#ifndef PROC_H
#define PROC_H

#include "types.h"

typedef enum {
    READY = 0,
    ACTIVE,
    ZOMBIE,
    NUM_STATES
} taskState_t;

typedef struct {
    taskState_t taskState;
    uint32_t tid;
    int parentTid;
    uint32_t priority;
    uint32_t sp;
} task_t;

/*
task_t *schedule(task_t *currentTask, taskQueue_t *taskQueue);
void contextSwitch(task_t *currentTask);
*/

#endif
