/*
 * syscall.c - System call functions
 */

#include <syscall_types.h>

// sp is a dummy input to pad r0, and is set in the asm function
extern int swi_call(int sp, void *args);

int MyTid() {
    k_args_t args;
    args.code = SYS_MYTID;
    return swi_call(0, &args);
}


int MyParentTid() {
    k_args_t args;
    args.code = SYS_PTID;
    return swi_call(0, &args);
}


int Create(int priority, void (*code) ()) {
    k_args_t args;
    args.code = SYS_CREATE;
    args.a0 = priority;
    args.a1 = (uint32_t) code;
    return swi_call(0, &args);
}


void Pass() {
    k_args_t args;
    args.code = SYS_PASS;
    swi_call(0, &args);
}


void Exit() {
    k_args_t args;
    args.code = SYS_EXIT;
    swi_call(0, &args);
}
