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
} TaskState_t;

typedef struct __task_t {
    TaskState_t state;
    uint32_t tid;
    int parentTid;
    int priority;
    uint32_t sp;
    struct __task_t *next;
    MemBlock_t * addrspace;
    // result of last request, TODO(max) - optimize this by using the stack
    uint32_t result;
} Task_t;

typedef struct {
    Task_t *head;
    Task_t *tail;
} TaskQueue_t;


void initTasks();
Task_t *createTaskD(uint32_t);
Task_t *getCurrentTask();
void destroyTaskD();
void addTask(Task_t*);
Task_t *schedule();

#endif /* __TASK_H__ */
