#include <task.h>
#include <term.h>
#define TASK_QUEUE_SIZE  16
#define TASK_BANK_SIZE   32

static task_queue taskQueue[TASK_QUEUE_SIZE];
static uint32_t nextTid;
static uint32_t bankPtr;
static task_t *currentTask;
static task_t __taskBank[TASK_BANK_SIZE];
static task_t *taskBank;

static int availableQueues;
static int highestTaskPriority;

void initTasks() {
    uint32_t i;

    bankPtr = 0;
    nextTid = 0;
    currentTask = NULL;
    availableQueues = 0;
    highestTaskPriority = -1;

    for (i = 0; i < TASK_BANK_SIZE; ++i) {
        __taskBank[i].state = FREE;
        if (i < TASK_BANK_SIZE - 1) {
            __taskBank[i].next = &__taskBank[i + 1];
        } else {
            __taskBank[i].next = NULL;
        }
    }

    for (i = 0; i < TASK_QUEUE_SIZE; ++i) {
        taskQueue[i].head = NULL;
        taskQueue[i].tail = NULL;
    }

    taskBank = &__taskBank[0];
}


task_t *createTaskD(uint32_t priority) {
    task_t *t = NULL;

    if (taskBank != NULL) {
        t = taskBank;
        taskBank = t->next;
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

    availableQueues |= 1 << t->priority;
    highestTaskPriority = t->priority > highestTaskPriority ? t->priority : highestTaskPriority;

#if DEBUG
    printf("added task with priority %d, new availableQueues: %x, new highestTaskPriority: %d\n", t->priority, availableQueues, highestTaskPriority);
#endif
}


void destroyTaskD() {
    freeMem(currentTask->addrspace);

    currentTask->state = FREE;
    currentTask->tid = 0;
    currentTask->parentTid = -1;
    currentTask->priority = -1;
    currentTask->sp = 0;
    currentTask->addrspace = NULL;
    currentTask->result = 0;

    /* add task back to bank */
    currentTask->next = taskBank;
    taskBank = currentTask;

    currentTask = NULL;
}

void findHighestTaskPriority() {
    if(availableQueues == 0) {
        highestTaskPriority = -1;
        return;
    }

#if DEBUG
    printf("availableQueues: 0x%x\n", availableQueues);
#endif

    int high, mid;

    for(high = highestTaskPriority; ; high = mid) {

        mid = high >> 1;

        if((1 << mid) <= availableQueues) {
            break;
        }
    }

    while( !((1 << high) & availableQueues)) {
        --high;
    }

    highestTaskPriority = high;
#if DEBUG
    printf("new high: %d\n", high);
#endif
}

task_t *schedule() {
    if (highestTaskPriority < 0) {
        return currentTask;
    }

    if (currentTask) {
        if (currentTask->priority >= highestTaskPriority) {
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
    if (nextTask == queue->tail) {
        queue->tail = NULL;
        availableQueues -= 1 << nextTask->priority;
        findHighestTaskPriority();
    }

    // set as current
    nextTask->next = NULL;
    nextTask->state = ACTIVE;
    currentTask = nextTask;

    return currentTask;
}
