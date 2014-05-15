#ifndef __SYSCALL__
#define __SYSCALL__
#define EGG               0
#define CREATE            1
#define MYTID             2
#define PTID              3
#define PASS              4
#define EXIT              5

int Create(int, void (*code)());
int MyTid();
int MyParentTid();
void Pass();
void Exit();

#endif
