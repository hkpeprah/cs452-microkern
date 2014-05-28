#include <task.h>
#include <term.h>
#define TASK_QUEUE_SIZE  16
#define TASK_BANK_SIZE   32
#define MESSAGE_QUEUE_SIZE 64

static TaskQueue_t taskQueue[TASK_QUEUE_SIZE];
static uint32_t nextTid;
static uint32_t bankPtr;
static Task_t *currentTask;
static Task_t __taskBank[TASK_BANK_SIZE];
static Envelope_t __envelopes[MESSAGE_QUEUE_SIZE] = {{0}};
static Envelope_t *availableEnvelopes;
static Task_t *taskBank;
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

        __taskBank[i].inboxHead = NULL;
        __taskBank[i].inboxTail = NULL;
    }

    for (i = 0; i < TASK_QUEUE_SIZE; ++i) {
        taskQueue[i].head = NULL;
        taskQueue[i].tail = NULL;
    }

    taskBank = &__taskBank[0];

    // link together message queue blocks
    Envelope_t *lastBlock = NULL;

    for(i = 0; i < MESSAGE_QUEUE_SIZE; ++i) {
        __envelopes[i].next = lastBlock;
        lastBlock = &__envelopes[i];
    }

    availableEnvelopes = lastBlock;
}

Envelope_t *getEnvelope() {
    if (availableEnvelopes == NULL) {
        return NULL;
    }

    Envelope_t *envelope = availableEnvelopes;
    availableEnvelopes = availableEnvelopes->next;
    envelope->next = NULL;
    return envelope;
}

void releaseEnvelope(Envelope_t *envelope) {
    envelope->next = availableEnvelopes;
    availableEnvelopes = envelope;
}


Task_t *createTaskD(uint32_t priority) {
    Task_t *t = NULL;

    if (taskBank != NULL && priority < TASK_QUEUE_SIZE) {
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

Task_t *getTaskByTid(uint32_t tid) {
    Task_t *task = &__taskBank[tid % TASK_BANK_SIZE];

    if (task->state == FREE || task->tid != tid) {
        return NULL;
    }

    return task;
}

Task_t *getCurrentTask() {
    return currentTask;
}


void addTask(Task_t *t) {
    TaskQueue_t *queue = &(taskQueue[t->priority]);

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

    debugf("Added task with priority %d\t New availableQueues: %x\t New highestTaskPriority: %d",
           t->priority, availableQueues, highestTaskPriority);
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
    currentTask->state = ZOMBIE;
    currentTask = NULL;
}


void findHighestTaskPriority() {
    int high, mid;

    if (availableQueues == 0) {
        highestTaskPriority = -1;
        return;
    }

    debugf("AvailableQueues: 0x%x", availableQueues);

    for (high = highestTaskPriority; ; high = mid) {
        mid = high >> 1;

        if ((1 << mid) <= availableQueues) {
            break;
        }
    }

    while (!((1 << high) & availableQueues)) {
        --high;
    }

    highestTaskPriority = high;
    debugf("New high: %d", high);
}

Task_t *schedule() {
    if (highestTaskPriority < 0) {
        return currentTask;
    }

    // only when current task exists and is ACTIVE (ie. it didn't just get blocked)
    if (currentTask && currentTask->state == ACTIVE) {
        if (currentTask->priority > highestTaskPriority) {
            // no highest task priority == empty queues, only current task available
            // current task has higher priority -> it should keep running
            return currentTask;
        }

        // add current task to queue
        addTask(currentTask);
    }

    // get queue with highest priority. this is guaranteed to not be empty
    TaskQueue_t *queue = &taskQueue[highestTaskPriority];

    // update the task queue
    Task_t *nextTask = queue->head;
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
