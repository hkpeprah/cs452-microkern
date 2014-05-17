#ifndef __TASK_H__
#define __TASK_H__
#include <types.h>
#include <mem.h>
#define REGS_SAVE    10

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
    MemBlock_t * addrspace;
} task_t;

typedef struct {
    task_t *head;
    task_t *tail;
} task_queue;


void initTasks();
task_t *createTaskD(uint32_t);
task_t *getLastTask();
task_t *getCurrentTask();
void destroyTaskD(task_t*);
void addTask(task_t*);
task_t *schedule();
void contextSwitch(task_t*);

#endif /* __TASK_H__ */
