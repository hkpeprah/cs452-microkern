#ifndef __K_SYSCALL__
#define __K_SYSCALL__

#include <types.h>

/* errors that can happen during syscalls */
#define OUT_OF_SPACE           1
#define TASK_DOES_NOT_EXIST    2

int sys_create(int, void (*)(), uint32_t*);
int sys_tid(uint32_t*);
int sys_pid(uint32_t*);
void sys_pass();
void sys_exit();
void syscall(unsigned int, void*);

#endif
