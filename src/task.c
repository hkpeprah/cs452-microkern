#include <task.h>
#include <term.h>
#include <logger.h>
#include <stdlib.h>
#define TASK_QUEUE_SIZE     (1 << PRIORITY_BITS)
#define TASK_BANK_SIZE      64
#define MESSAGE_QUEUE_SIZE  64

static TaskQueue_t taskQueue[TASK_QUEUE_SIZE];
static uint32_t bankPtr;
static Task_t *currentTask;
static Task_t __taskBank[TASK_BANK_SIZE];
static Envelope_t __envelopes[MESSAGE_QUEUE_SIZE] = {{0}};
static Envelope_t *availableEnvelopes;
static Task_t *taskBank;
static int availableQueues;
static int highestTaskPriority;

static void findHighestTaskPriority();
#if LOG
static void queueState(int priority);
#endif

static const char *STATES[] = {"READY", "ACTIVE", "ZOMBIE", "FREE", "SEND_BL", "RECV_BL", "REPLY_BL"
                               "EVENT_BL", "WAITTID_BL"};

void initTasks() {
    uint32_t i;

    bankPtr = 0;
    currentTask = NULL;
    availableQueues = 0;
    highestTaskPriority = -1;

    for (i = 0; i < TASK_BANK_SIZE; ++i) {
        __taskBank[i].state = FREE;
        __taskBank[i].tid = i;
        if (i < TASK_BANK_SIZE - 1) {
            __taskBank[i].next = &__taskBank[i + 1];
        } else {
            __taskBank[i].next = NULL;
        }

        __taskBank[i].inboxHead = NULL;
        __taskBank[i].inboxTail = NULL;
        __taskBank[i].waitQueue = NULL;
        __taskBank[i].outbox = NULL;
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

    priority = priority >= TASK_QUEUE_SIZE ? TASK_QUEUE_SIZE - 1 : priority;
    if (taskBank != NULL) {
        t = taskBank;
        taskBank = t->next;
        t->parentTid = (currentTask == NULL) ? -1 : currentTask->tid;
        t->priority = priority;
        t->addrspace = getMem();
        t->sp = t->addrspace->addr;
        t->next = NULL;
        t->inboxHead = NULL;
        t->inboxTail = NULL;
        t->outbox = NULL;
        addTask(t);
    }

    return t;
}


Task_t *getTaskByTid(uint32_t tid) {
    Task_t *task = &__taskBank[tid & (TASK_BANK_SIZE - 1)];

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
    #if DEBUG_KERNEL
        kdebug("Added task with priority %d\t New availableQueues: %x\t New highestTaskPriority: %d",
               t->priority, availableQueues, highestTaskPriority);
    #endif
}


void destroyTaskD(Task_t *task) {
    Task_t *t;
    Task_t *prev = NULL, *cur = NULL;
    TaskQueue_t *queue;
    Envelope_t *env;

    t = task->waitQueue;
    while (t != NULL) {
        setResult(t, task->tid);
        addTask(t);
        t = t->next;
    }

    freeMem(task->addrspace);

    if (task->state == READY) {
        queue = &(taskQueue[task->priority]);
        cur = queue->head;

        while (cur != NULL && cur != task) {
            prev = cur;
            cur = cur->next;
        }

        if (cur == NULL) {
            error("Kernel: destroyTaskD: could not find task %d on priority queue %d\n", task->tid, task->priority);
        }

        // at this point, cur == task
        if (prev) {
            prev->next = cur->next;
        } else {
            queue->head = cur->next;
        }

        if (cur == queue->tail) {
            queue->tail = prev;
        }

        if (queue->head == NULL) {
            availableQueues -= 1 << task->priority;
            if (task->priority == highestTaskPriority) {
                findHighestTaskPriority();
            }
        }
        #if LOG
            queueState(task->priority);
        #endif
    }

    task->state = FREE;
    task->tid += TASK_BANK_SIZE;
    task->parentTid = 0;
    task->priority = 0;
    task->sp = 0;
    task->addrspace = NULL;

    while ( (env = task->inboxHead) != NULL) {
        task->inboxHead = task->inboxHead->next;
        t = UNION_CAST(env->sender, Task_t*);

        addTask(t);
        releaseEnvelope(env);
        t->outbox = NULL;

        setResult(t, TASK_DESTROYED);
    }

    task->inboxTail = NULL;
    if (task->outbox != NULL) {
        releaseEnvelope(task->outbox);
    }
    task->outbox = NULL;

    if (currentTask == task) {
        currentTask = NULL;
    }

    task->next = taskBank;
    taskBank = task;
}


#if LOG
static void queueState(int priority) {
    Task_t *t;

    sys_log_f("priority {%d} queue: ", priority);
    t = taskQueue[priority].head;
    while (t) {
        sys_log_f("{%d} ", t->tid);
        t = t->next;
    }
    sys_log_f("availableQueues: 0x%x, highestTaskPriority: %d\n", availableQueues, highestTaskPriority);

}
#endif


static void findHighestTaskPriority() {
    int high, mid;

    if (availableQueues == 0) {
        highestTaskPriority = -1;
        return;
    }

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
}


Task_t *schedule() {
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

    if (highestTaskPriority < 0) {
        // no more tasks to be run
        return NULL;
    }

    #if LOG
        queueState(highestTaskPriority);
    #endif

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

void setResult(Task_t *task, int result) {
    if (task->sp <= 0x00218000) {
        kprintf("KERNEL: FATAL: trying to set result on tid {%d} sp {%d}\n", task->tid, task->sp);
        return;
    }
    int *sp = (int*)task->sp;
    sp[2] = result;
}

void dumpTaskState() {
    int prio = highestTaskPriority;
    while (prio --> 0) {
        Task_t *task = taskQueue[prio].head;
        if (!task) {
            break;
        }

        kprintf("Priority %d:\n", prio);
        while (task) {
            kprintf("Tid: {%d} Parent: {%d} State: {%s} SP: {%d}", task->tid, task->parentTid, STATES[task->state], task->sp);
            task = task->next;
        }
    }
}
