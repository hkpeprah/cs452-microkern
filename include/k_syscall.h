#ifndef __K_SYSCALL__
#define __K_SYSCALL__
#include <types.h>
#include <term.h>


int sys_create(int, void (*)(), uint32_t*);
int sys_tid(uint32_t*);
int sys_pid(uint32_t*);
int sys_send(int tid, void *msg, int msglen, void *reply, int replylen);
int sys_recv(int *tid, void *msg, int msglen);
int sys_reply(int tid, void *reply, int replylen);
void sys_pass();
void sys_exit();
int sys_destroy(uint32_t tid);
int sys_await(int, void*, int);
int sys_waittid(uint32_t);
void sys_idle(uint32_t*);
void sys_sigterm();
void sys_panic(char *message, va_list va);

#endif /* __K_SYSCALL__ */
