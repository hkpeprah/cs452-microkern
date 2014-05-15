<<<<<<< Updated upstream:include/task.h
#ifndef __TASK_H__
#define __TASK_H__
#include <types.h>
#include <mem.h>

typedef enum {
    READY = 0,
    ACTIVE,
    ZOMBIE,
    NUM_STATES
} taskState_t;

typedef struct {
    taskState_t state;
    uint32_t tid;
    int parentTid;
    uint32_t priority;
    uint32_t sp;
} task_t;


task_t currentTask = NULL;

task_t *schedule(task_t *currentTask, taskQueue_t *taskQueue);
void contextSwitch(task_t *currentTask);

#endif /* __TASK_H__ */
