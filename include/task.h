#ifndef __TASK_H__
#define __TASK_H__
#include <types.h>
#include <mem.h>

#define PRIORITY_BITS 4

struct __task_t;

typedef enum {
    READY = 0,
    ACTIVE,
    ZOMBIE,
    FREE,
    SEND_BL,
    RECV_BL,
    REPL_BL,
    EVENT_BL,
    WAITTID_BL,
    NUM_STATES
} TaskState_t;


struct __envelope_t;


typedef struct __task_t {
    TaskState_t state;
    unsigned int tid : 13;
    int parentTid : 14;
    uint32_t priority : PRIORITY_BITS;
    uint32_t sp;
    struct __task_t *next;
    struct __task_t *waitQueue;
    MemBlock_t * addrspace;
    struct __envelope_t *inboxHead;
    struct __envelope_t *inboxTail;
    struct __envelope_t *outbox;    // information for reply (Sender case) or receive (blocked Receive case)
} Task_t;


typedef struct {
    Task_t *head;
    Task_t *tail;
} TaskQueue_t;

typedef struct __envelope_t {
    void *msg;
    int msglen;
    void *reply;
    int replylen;
    void *sender;   // Task_t* when it is sent, int* when it is attached to a blocked receiver (used to write the sender tid into)
    struct __envelope_t *next;
} Envelope_t;


void initTasks();
Envelope_t *getEnvelope();
void releaseEnvelope(Envelope_t *envelope);
Task_t *createTaskD(uint32_t);
Task_t *getTaskByTid(uint32_t tid);
Task_t *getCurrentTask();
void destroyTaskD();
void addTask(Task_t*);
Task_t *schedule();
void setResult(Task_t*, int);
void dumpTaskState();

#endif /* __TASK_H__ */
