#ifndef __SYSCALL__
#define __SYSCALL__


int Create(int, void(*)());
int MyTid();
int MyParentTid();
void Pass();
void Exit();
int Send(int, void*, int, void*, int);
int Receive(int*, void*, int);
int Reply(int, void*, int);
int AwaitEvent(int, void*, int);
int WaitTid(unsigned int);
int Log(const char *fmt, ...);

#endif /* __SYSCALL__ */
