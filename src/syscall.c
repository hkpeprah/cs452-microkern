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
    args.code = SYS_RECV;
    args.a0 = (uint32_t)tid;
    args.a1 = (uint32_t)msg;
    args.a2 = msglen;
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


int AwaitEvent(int eventType, void *buf, int buflen) {
    Args_t args;
    args.code = SYS_AWAIT;
    args.a0 = eventType;
    args.a1 = (uint32_t) buf;
    args.a2 = buflen;
    return swi_call(0, &args);
}


int WaitTid(unsigned int tid) {
    Args_t args;
    args.code = SYS_WAITTID;
    args.a0 = tid;
    return swi_call(0, &args);
}

int Logn(const char *str, int n) {
    Args_t args;
    args.code = SYS_LOG;
    args.a0 = (uint32_t) str;
    args.a1 = n;

    return swi_call(0, &args);
}

int Log(const char *fmt, ...) {
    int len;
    char buffer[256];
    va_list va;

    va_start(va, fmt);
    len = format(fmt, va, buffer);
    va_end(va);

    return Logn(buffer, len);
}


int CpuIdle() {
    Args_t args;
    args.code = SYS_IDLE;
    return swi_call(0, &args);
}
