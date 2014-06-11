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

/*
 * Waits for an external event.  Blocks until the event defined by the
 * passed parameter occurs then returns.
 * Returns
 *     0 - event returned in buffer
 *    -1 - invalid event
 *    -2 - event queue full
 *    Otherwise returns volatile data
 */
int AwaitEvent(int, void*, int);

int WaitTid(unsigned int);

#endif /* __SYSCALL__ */
