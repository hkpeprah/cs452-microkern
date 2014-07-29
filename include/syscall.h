#ifndef __SYSCALL__
#define __SYSCALL__
#include <types.h>

int Create(int, void(*)());
int MyTid();
int MyParentTid();
void Pass();
void Exit();
int Destroy(uint32_t tid);
int Send(int tid, void*, int, void*, int);
int Receive(int*, void*, int);
int Reply(int, void*, int);
int AwaitEvent(int, void*, int);
int WaitTid(unsigned int);
int Logn(const char *str, int n);
int Log(const char *fmt, ...);
int CpuIdle();
void SigTerm(int status);
void Panic(char *, ...);
void Assert(char *assert_msg, uint32_t line, char *file, const char *func, char *msg, ...);

#endif /* __SYSCALL__ */
