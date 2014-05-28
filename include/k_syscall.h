#ifndef __K_SYSCALL__
#define __K_SYSCALL__

#include <types.h>

/* errors that can happen during syscalls */
#define OUT_OF_SPACE            -1
#define TASK_DOES_NOT_EXIST     -2      // tried to access a task that doesn't exist
#define NO_MORE_ENVELOPES       -3      // out of envelopes... oh no!
#define NO_AVAILABLE_MESSAGES   -4      // task called receive, but no messages, blocked
#define MISMATCHED_MESSAGE_LEN  -5      // incorrect length between send/receive calls
#define TASK_NOT_EXPECTING_MSG  -6      // tried to reply to a task which didn't send

int sys_create(int, void (*)(), uint32_t*);
int sys_tid(uint32_t*);
int sys_pid(uint32_t*);
int sys_send(int tid, void *msg, int msglen, void *reply, int replylen);
int sys_recv(int *tid, void *msg, int msglen);
int sys_reply(int tid, void *reply, int replylen);
void sys_pass();
void sys_exit();

#endif /* __K_SYSCALL__ */
