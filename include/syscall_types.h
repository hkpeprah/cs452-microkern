#ifndef __SYSCALL_DEFS__
#define __SYSCALL_DEFS__
#include <types.h>


typedef enum {
    SYS_MYTID = 0,
    SYS_PTID,
    SYS_EGG,
    SYS_CREATE,
    SYS_PASS,
    SYS_EXIT,
    SYS_SEND,
    SYS_RECV,
    SYS_REPL,
    SYS_AWAIT,
    SYS_INTERRUPT,
    SYS_WAITTID
} system_calls;

typedef struct args {
    system_calls code;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
} Args_t;

#endif /* __SYSCALL_DEFS__ */
