#ifndef __TASK_H__
#define __TASK_H__
#include <types.h>
#include <mem.h>

struct __task_t;

typedef enum {
    READY = 0,
    ACTIVE,
    ZOMBIE,
    FREE,
    NUM_STATES
} taskState_t;

typedef struct __task_t {
    taskState_t state;
    uint32_t tid;
    int parentTid;
    uint32_t priority;
    uint32_t sp;
    struct __task_t *next;
} task_t;

typedef struct {
    task_t *head;
    task_t *tail;
} task_queue;

extern task_t *currentTask;


void initTask();
task_t *createTaskD(int priority);
task_t *schedule();
void contextSwitch(task_t*);

#endif /* __TASK_H__ */
