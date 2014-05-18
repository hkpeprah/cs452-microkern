#ifndef __SYSCALL_DEFS__
#define __SYSCALL_DEFS__
#include <types.h>

typedef enum {
    SYS_MYTID = 0,
    SYS_PTID,
    SYS_EGG,
    SYS_CREATE,
    SYS_PASS,
    SYS_EXIT
} system_calls;

typedef struct k_args {
    system_calls code;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
} k_args_t;

#endif /* __SYSCALL_DEFS__ */
