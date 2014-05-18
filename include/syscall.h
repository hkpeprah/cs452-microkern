#ifndef __SYSCALL__
#define __SYSCALL__

int Create(int, void(*)());
int MyTid();
int MyParentTid();
void Pass();
void Exit();

#endif /* __SYSCALL__ */
