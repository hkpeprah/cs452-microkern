#include <mem.h>
#include <k_syscall.h>
#include <task.h>
#include <types.h>
#include <stdlib.h>
#include <term.h>

#define INIT_SPSR   0x13c0
#define REGS_SAVE   11


int sys_create(int priority, void (*code)(), uint32_t *retval) {
    Task_t *task;
    unsigned int i;
    uint32_t *sp;

    task = createTaskD(priority);

    if (task != NULL) {
        /* save the sp, lr and regs 1 - 9 */
        sp = (uint32_t*)task->sp;
        *(--sp) = (uint32_t)code;
        for (i = 0; i < REGS_SAVE; ++i) {
            *(--sp) = 0;
        }

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
    if(current != NULL) {
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

    return 0;
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

    // mainly used for debug... mismatched message len = oh no bad stuff
    if (envelope->msglen != msglen) {
        return MISMATCHED_MESSAGE_LEN;
    }

    memcpy(msg, envelope->msg, msglen);
    *tid = envelope->sender->tid;
    envelope->sender->state = REPL_BL;

    return envelope->msglen;
}

int sys_reply(int tid, void *reply, int replylen) {
    Task_t *target = getTaskByTid(tid);

    if (target == NULL) {
        return TASK_DOES_NOT_EXIST;
    }

    Envelope_t *envelope = target->outbox;

    if (envelope == NULL || target->state != REPL_BL) {
        return TASK_NOT_EXPECTING_MSG;
    }

    if (envelope->replylen != replylen) {
        return MISMATCHED_MESSAGE_LEN;
    }
    memcpy(envelope->reply, reply, replylen);

    addTask(target);
    releaseEnvelope(envelope);
    return replylen;
}

void sys_pass() {
    /* do nothing */
}


void sys_exit() {
    destroyTaskD();
}
