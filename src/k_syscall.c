#include <mem.h>
#include <k_syscall.h>
#include <syscall.h>
#include <task.h>
#include <types.h>
#include <stdlib.h>
#include <term.h>
#include <util.h>
#include <interrupt.h>

#define INIT_SPSR   0x10
#define REGS_SAVE   13


int sys_create(int priority, void (*code)(), uint32_t *retval) {
    Task_t *task;
    unsigned int i;
    uint32_t *sp;

    task = createTaskD(priority);

    if (task != NULL) {
        sp = (uint32_t*)task->sp;
        *(--sp) = (uint32_t)Exit;
        for (i = 0; i < REGS_SAVE; ++i) {
            *(--sp) = 0;
        }

        *(--sp) = (uint32_t)code;
        *(--sp) = INIT_SPSR;
        task->sp = (uint32_t)sp;
        *retval = task->tid;

        return 0;
    }
    return OUT_OF_SPACE;
}


int sys_tid(uint32_t *retval) {
    Task_t *current = getCurrentTask();
    if (current != NULL) {
        *retval = current->tid;
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}


int sys_pid(uint32_t *retval) {
    Task_t *current = getCurrentTask();
    if (current != NULL) {
        *retval = current->parentTid;
        return 0;
    }
    return TASK_DOES_NOT_EXIST;
}


int sys_send(int tid, void *msg, int msglen, void *reply, int replylen) {
    Task_t *currentTask = getCurrentTask();
    Task_t *target = getTaskByTid(tid);

    if (target == NULL) {
        return TASK_DOES_NOT_EXIST;
    }

    Envelope_t *envelope = getEnvelope();

    if (envelope == NULL) {
        return NO_MORE_ENVELOPES;
    }

    // copy parameters into envelope struct
    envelope->msg = msg;
    envelope->msglen = msglen;
    envelope->reply = reply;
    envelope->replylen = replylen;
    envelope->sender = currentTask;

    // stored here for reply
    currentTask->outbox = envelope;

    // block current
    currentTask->state = RECV_BL;

    // add envelope to receiver's queue
    if (target->inboxHead == NULL) {
        target->inboxHead = envelope;
    } else if (target->inboxTail != NULL) {
        target->inboxTail->next = envelope;
    }

    target->inboxTail = envelope;

    // unblock receiver if they are blocked
    if (target->state == SEND_BL) {
        addTask(target);
    }

    // value does not matter, will be overwritten during reply
    return msglen;
}


int sys_recv(int *tid, void *msg, int msglen) {
    Task_t *currentTask = getCurrentTask();
    Envelope_t *envelope = currentTask->inboxHead;

    // no messages available, block & return error
    if (envelope == NULL) {
        currentTask->state = SEND_BL;
        return NO_AVAILABLE_MESSAGES;
    }

    // otherwise, pop head
    currentTask->inboxHead = envelope->next;
    envelope->next = NULL;

    if (currentTask->inboxHead == NULL) {
        currentTask->inboxTail = NULL;
    }

    memcpy(msg, envelope->msg, MIN(msglen, envelope->msglen));
    *tid = envelope->sender->tid;
    envelope->sender->state = REPL_BL;

    return envelope->msglen;
}


int sys_reply(int tid, void *reply, int replylen) {
    /* returns 0 on success */
    Task_t *target = getTaskByTid(tid);

    if (target == NULL) {
        return TASK_DOES_NOT_EXIST;
    }

    Envelope_t *envelope = target->outbox;

    if (envelope == NULL || target->state != REPL_BL) {
        return TASK_NOT_REPLY_BLOCKED;
    }

    memcpy(envelope->reply, reply, MIN(replylen, envelope->replylen));

    addTask(target);
    releaseEnvelope(envelope);
    target->outbox = NULL;

    // set result of send to the length of the reply
    setResult(target, replylen);
    return 0;
}


void sys_pass() {
    /* do nothing */
}


void sys_exit() {
    destroyTaskD();
}


int sys_await(int eventType) {
    /*
     * Blocks the task on the given eventType, then resume
     * when the event occurs, to wake up the given task.
     */
    int errno;
    Task_t *target = getCurrentTask();
    target->state = EVENT_BL;
    errno = addInterruptListener(eventType, target);

    if (errno < 0) {
        debugf("AwaitEvent: Got %d adding task %d", errno, target->tid);
    }

    return errno;
}
