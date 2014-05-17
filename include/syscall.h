#ifndef __SYSCALL__
#define __SYSCALL__
#include <types.h>

/* errors that can happen during syscalls */
#define OUT_OF_SPACE           1
#define TASK_DOES_NOT_EXIST    2


typedef enum {
    SYS_MYTID = 0,
    SYS_PTID,
    SYS_EGG,
    SYS_CREATE,
    SYS_PASS,
    SYS_EXIT
} system_calls;


void syscall(unsigned int);
int Create(int, void(*)());
int MyTid();
int MyParentTid();
void Pass();
void Exit();

#endif
