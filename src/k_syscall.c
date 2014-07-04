#include <mem.h>
#include <k_syscall.h>
#include <syscall.h>
#include <task.h>
#include <types.h>
#include <stdlib.h>
#include <term.h>
#include <util.h>
#include <interrupt.h>
#include <idle.h>
#include <null.h>

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
    Envelope_t *envelope;

    if (target == NULL) {
        return TASK_DOES_NOT_EXIST;
    }

    if (target == currentTask) {
        kprintf("Send: Error: %d sending to self, msg: 0x%x, len: %d\r\n", tid, msg, msglen);
        return TASK_ID_IMPOSSIBLE;
    }

    if (target->state == SEND_BL && target->outbox != NULL) {
        /*
         * case 1: target already blocked on receive, so directly copy to it
         * use the "outbox" to store the envelope when it blocks. A hack to
         * save some space in our task descriptors
         */
        envelope = target->outbox;
        target->outbox = NULL;

        // copy into it
        memcpy(envelope->msg, msg, MIN(msglen, envelope->msglen));
        *(UNION_CAST(envelope->sender, int*)) = currentTask->tid;
        setResult(target, msglen);

        // set up reply destination
        envelope->reply = reply;
        envelope->replylen = replylen;
        envelope->sender = currentTask;

        // take over envelope!
        currentTask->outbox = envelope;
        currentTask->state = REPL_BL;
    } else {
        /*
         * case 2: not blocked on receive -> build envelope and add it to target's inbound
         * queue
         */
        envelope = getEnvelope();

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
    }

    if(target->state == SEND_BL) {
        addTask(target);
    }

    return msglen;
}


int sys_recv(int *tid, void *msg, int msglen) {
    Task_t *currentTask = getCurrentTask();
    Envelope_t *envelope;

    /*
     * no messages available, copy info into envelope, block & return error
     * when this is unblocked by send, the result should be updated by it
     */
    if (currentTask->inboxHead == NULL) {
        currentTask->state = SEND_BL;
        envelope = getEnvelope();

        if(envelope == NULL) {
            return NO_MORE_ENVELOPES;
        }

        envelope->msg = msg;
        envelope->msglen = msglen;
        envelope->sender = tid;

        currentTask->outbox = envelope;

        return NO_AVAILABLE_MESSAGES;
    }

    envelope = currentTask->inboxHead;

    // otherwise, pop head
    currentTask->inboxHead = envelope->next;
    envelope->next = NULL;

    if (currentTask->inboxHead == NULL) {
        currentTask->inboxTail = NULL;
    }

    Task_t *sender = UNION_CAST(envelope->sender, Task_t*);

    memcpy(msg, envelope->msg, MIN(msglen, envelope->msglen));
    *tid = sender->tid;
    sender->state = REPL_BL;

    return envelope->msglen;
}


int sys_reply(int tid, void *reply, int replylen) {
    /* returns 0 on success */
    Envelope_t *envelope;
    Task_t *target = getTaskByTid(tid);

    if (target == NULL) {
        return TASK_DOES_NOT_EXIST;
    }

    envelope = target->outbox;
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


int sys_await(int eventType, void *buf, int buflen) {
    /*
     * Blocks the task on the given eventType, then resume
     * when the event occurs, to wake up the given task.
     */
    int errno;
    Task_t *target = getCurrentTask();
    errno = addInterruptListener(eventType, target, buf, buflen);

    if (errno < 0) {
        error("AwaitEvent: Error: Got %d adding task %d", errno, target->tid);
    } else {
        target->state = EVENT_BL;
    }

    return errno;
}


int sys_waittid(uint32_t tid) {
    Task_t *target;
    Task_t *currentTask;

    target = getTaskByTid(tid);
    currentTask = getCurrentTask();

    if (target == NULL || currentTask->tid != tid) {
        error("WaitTid: Task does not exist or exited already.");
        return TASK_DOES_NOT_EXIST;
    }

    currentTask->state = WAITTID_BL;
    currentTask->next = target->waitQueue;
    target->waitQueue = currentTask;

    return tid;
}


void sys_idle(uint32_t *retval) {
    *retval = getIdleTime();
}


void sys_sigterm() {
    setExit(1);
}
