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
    SEND_BL,
    RECV_BL,
    REPL_BL,
    NUM_STATES
} TaskState_t;

struct __envelope_t;


typedef struct __task_t {
    TaskState_t state;
    uint32_t tid : 8;
    uint32_t parentTid : 8;
    int priority;
    uint32_t sp;
    struct __task_t *next;
    MemBlock_t * addrspace;
    // result of last request, TODO(max) - optimize this by using the stack
    uint32_t result;
    struct __envelope_t *inboxHead;
    struct __envelope_t *inboxTail;
    struct __envelope_t *outbox;
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
    Task_t *sender;
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

#endif /* __TASK_H__ */
