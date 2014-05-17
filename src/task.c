#include <task.h>
#define TASK_QUEUE_SIZE  16
#define TASK_BANK_SIZE   32

static task_queue taskQueue[TASK_QUEUE_SIZE];
static task_t taskBank[TASK_BANK_SIZE];
static uint32_t bankPtr = 0;
static uint32_t nextTid = 0;
task_t *currentTask = NULL;


void initTasks() {
    uint32_t i;
    task_t * t;

    for (i = 0; i < TASK_BANK_SIZE; ++i) {
        t = &(taskBank[i]);
        t->state = READY;
        t->next = NULL;
    } 
}


task_t *createTaskD(int priority) {
    task_t *t = NULL;
    uint32_t i = bankPtr;

    do {
        if (taskBank[i].state == FREE) {
            t = &(taskBank[i]);
            break;
        }
    } while (++i != bankPtr);

    if (t != NULL) {
        bankPtr = (i + 1) % TASK_BANK_SIZE;
        t->state = READY;
        t->sp = 0;
        t->next = NULL;
        t->addrspace = getMem();
        t->tid = nextTid++;
        addTask(t);
   }

    return t;
}


void addTask(task_t *t) {
    task_queue *queue = &(taskQueue[t->priority]);
    queue->head = queue->head ? queue->head : t;
    t->next = queue->head;
    if (queue->tail) {
        /* if queue tail exists, its next is this task */
        queue->tail->next = t;
    }
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

    i = (currentTask == NULL) ? TASK_QUEUE_SIZE : currentTask->priority;
    while (i >= 0) {
        if (taskQueue[i].head) break;
        --i;
    }

    if (i > -1) {
        queue = &(taskQueue[i]);
        t = queue->head;
        queue->head = t->next;
        queue->tail = (t == queue->tail) ? NULL : queue->tail;
        t->next = NULL;
        t->state = ACTIVE;
    }

    return t;
}


void contextSwitch(task_t *t) {
    /* NOP Placeholder */
    t = t;
}
