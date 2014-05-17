#include <task.h>
#include <term.h>
#define TASK_QUEUE_SIZE  16
#define TASK_BANK_SIZE   32

static task_queue taskQueue[TASK_QUEUE_SIZE];
static uint32_t nextTid = 0;
static uint32_t bankPtr;
static task_t *currentTask = NULL;
static task_t taskBank[TASK_BANK_SIZE];


void initTasks() {
    uint32_t i;

    bankPtr = 0;
    nextTid = 0;
    currentTask = NULL;

    for (i = 0; i < TASK_BANK_SIZE; ++i) {
        taskBank[i].state = FREE;
        taskBank[i].next = NULL;
    }

    for (i = 0; i < TASK_QUEUE_SIZE; ++i) {
        taskQueue[i].head = NULL;
        taskQueue[i].tail = NULL;
    }
}


task_t *createTaskD(uint32_t priority) {
    task_t *t = NULL;
    uint32_t i = bankPtr;

    do {
        i %= TASK_BANK_SIZE;
        if (taskBank[i].state == FREE) {
            t = &(taskBank[i]);
            break;
        }
    } while (++i != bankPtr);

    if (t != NULL) {
        bankPtr = i + 1;
        t->next = NULL;
        t->addrspace = getMem();
        t->sp = t->addrspace->addr;
        t->tid = nextTid++;
        t->priority = priority;
        addTask(t);
   }

    return t;
}


task_t *getCurrentTask() {
    return currentTask;
}


task_t *getLastTask() {
    return &(taskBank[bankPtr - 1]);
}


void addTask(task_t *t) {
    task_queue *queue = &(taskQueue[t->priority]);

    if (!queue->head) {
        queue->head = t;
    }

    if (queue->tail) {
        /* if queue tail exists, its next is this task */
        queue->tail->next = t;
    }

    t->next = NULL;
    t->state = READY;
    queue->tail = t;
}


void destroyTaskD(task_t *t) {
    freeMem(t->addrspace);
    t->state = FREE;
    t->tid = 0;
    t->sp = 0;
    t->addrspace = NULL;
}


task_t *schedule() {
    /* grabs the next scheduled task on the priority queue descending
       returns task_t if next task exists otherwise NULL */
    int32_t i;
    task_t *t = NULL;
    task_queue *queue;

    if (currentTask == NULL) {
        i = TASK_QUEUE_SIZE - 1;
    } else {
        i = currentTask->priority;
    }

    do {
        if (taskQueue[i].head != NULL) {
            queue = &(taskQueue[i]);
            break;
        }
    } while (--i >= 0);

    if (i > -1) {
        queue = &(taskQueue[i]);
        t = queue->head;
        queue->head = t->next;
        t->next = NULL;
        t->state = ACTIVE;
        if (t == queue->tail) queue->tail = NULL;
    }

    return t;
}


void contextSwitch(task_t *t) {
    /* NOP Placeholder */
    t = t;
}
