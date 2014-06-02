/*
 * syscall.c - System call functions
 */
#include <syscall_types.h>
#include <term.h>

// sp is a dummy input to pad r0, and is set in the asm function
extern int swi_call(int sp, void *args);


int MyTid() {
    Args_t args;
    args.code = SYS_MYTID;
    return swi_call(0, &args);
}


int MyParentTid() {
    Args_t args;
    args.code = SYS_PTID;
    return swi_call(0, &args);
}


int Create(int priority, void (*code) ()) {
    Args_t args;
    args.code = SYS_CREATE;
    args.a0 = priority;
    args.a1 = (uint32_t)code;
    return swi_call(0, &args);
}


void Pass() {
    Args_t args;
    args.code = SYS_PASS;
    swi_call(0, &args);
}


void Exit() {
    Args_t args;
    args.code = SYS_EXIT;
    swi_call(0, &args);
}


int Send(int tid, void *msg, int msglen, void *reply, int replylen) {
    Args_t args;
    args.code = SYS_SEND;
    args.a0 = tid;
    args.a1 = (uint32_t)msg;
    args.a2 = msglen;
    args.a3 = (uint32_t)reply;
    args.a4 = replylen;
    return swi_call(0, &args);
}


int Receive(int *tid, void *msg, int msglen) {
    Args_t args;
    int result;
    args.code = SYS_RECV;
    args.a0 = (uint32_t)tid;
    args.a1 = (uint32_t)msg;
    args.a2 = msglen;

    /*
     * optimistic call - if no messages available, the task
     * will be blocked until message is on its queue, at which
     * point it will need to make another call to get the message
     */
    result = swi_call(0, &args);

    if(result != NO_AVAILABLE_MESSAGES) {
        return result;
    }

    return swi_call(0, &args);
}


int Reply(int tid, void *reply, int replylen) {
    Args_t args;
    args.code = SYS_REPL;
    args.a0 = tid;
    args.a1 = (uint32_t)reply;
    args.a2 = replylen;
    return swi_call(0, &args);
}


int AwaitEvent(int eventType) {
    /*
     * Waits for an external event.  Blocks until the event defined by the
     * passed parameter occurs then returns.
     * Returns
     *    0  - volatie data in event buffer
     *    -1 - invalid event
     *    -2 - corrupted data, error in event buffer
     *    -3 - data must be collected, interrupts re-enabled in caller
     *    Otherwise returns volatile data
     */

    return 0;
}
