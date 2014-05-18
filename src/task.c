#include <task.h>
#include <term.h>
#define TASK_QUEUE_SIZE  16
#define TASK_BANK_SIZE   32

static task_queue taskQueue[TASK_QUEUE_SIZE];
static uint32_t nextTid = 0;
static uint32_t bankPtr;
static task_t *currentTask = NULL;
static task_t taskBank[TASK_BANK_SIZE];
static int highestTaskPriority;

void initTasks() {
    uint32_t i;

    bankPtr = 0;
    nextTid = 0;
    currentTask = NULL;
    highestTaskPriority = -1;

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

        t->tid = nextTid++;
        t->parentTid = (currentTask == NULL) ? -1 : currentTask->tid;
        t->priority = priority;
        t->addrspace = getMem();
        t->sp = t->addrspace->addr;
        t->next = NULL;
        t->result = -1;
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

    highestTaskPriority = t->priority > highestTaskPriority ? t->priority : highestTaskPriority;
}


void destroyTaskD() {
    freeMem(currentTask->addrspace);

    currentTask->state = FREE;
    currentTask->tid = 0;
    currentTask->parentTid = -1;
    currentTask->priority = -1;
    currentTask->sp = 0;
    currentTask->next = NULL;
    currentTask->addrspace = NULL;
    currentTask->result = 0;

    currentTask = NULL;
}


task_t *schedule() {
    if(highestTaskPriority < 0) {
        return NULL;
    }

    if(currentTask) {
        if(currentTask->priority >= highestTaskPriority) {
            // no highest task priority == empty queues, only current task available
            // current task has higher priority -> it should keep running
            return currentTask;
        }

        // add current task to queue
        currentTask->state = READY;
        addTask(currentTask);
    }

    // get queue with highest priority. this is guaranteed to not be empty
    task_queue *queue = &taskQueue[highestTaskPriority];

    // update the task queue
    task_t *nextTask = queue->head;
    queue->head = nextTask->next;

    // current priority queue is empty, find next highest priority
    if(nextTask == queue->tail) {
        queue->tail = NULL;

        do {
            --highestTaskPriority;
        } while(taskQueue[highestTaskPriority].head == NULL && highestTaskPriority >= 0);
    }

    // set as current
    nextTask->next = NULL;
    nextTask->state = ACTIVE;
    currentTask = nextTask;

    return currentTask;
}


void contextSwitch(task_t *t) {
    /* NOP Placeholder */
    t = t;
}
