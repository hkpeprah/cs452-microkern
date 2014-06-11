/*
 * utasks.c - User tasks for second part of kernel
 * First Task
 *    - Bootstraps the other tasks and the clients.
 *    - Quits when it has gone through all the clients.
 *
 * Client
 *    - Created by first user task, calls parent to get delay time t, and number of
 *      delays, n.
 *    - Uses WhoIs to discover Clock Server.
 *    - Delays n times, each for time interval, t, and prints after each.
 */
#include <term.h>
#include <utasks.h>
#include <syscall.h>
#include <shell.h>


void firstTask() {
    Create(15, NameServer);
    Create(15, ClockServer);
    Create(0, NullTask);
    Create(1, Shell);

    debug("FirstTask: Exiting");
    Exit();
}


void testTask() {
    printf("Calling task with priority: %d\r\n", getCurrentTask()->priority);
    Exit();
}


static void Client() {
    unsigned int pTid;
    unsigned int tid;
    int errno;
    DelayMessage msg, res;

    pTid = MyParentTid();
    msg.complete = 0;
    errno = Send(pTid, &msg, sizeof(msg), &res, sizeof(res));
    tid = MyTid();

    if (errno >= 0) {
        /*
         * Client delays n times, each for a time interval of t, printing after
         * each before exiting.
         */
        debug("Client: Task %d with (Interval %d, Number of Delays %d).", tid, res.t, res.n);
        while (res.n--) {
            Delay(res.t);
            res.complete += 1;
            printf("Tid: %d      Delay Interval: %d     Delays Complete: %d\r\n", tid, res.t, res.complete);
        }
    } else {
        error("Client: Error: Task %d received status %d sending to %d", tid, errno, pTid);
    }

    Exit();
}


void testTask2() {
    int i;
    int callee;
    DelayMessage msg, res;
    int priorities[] = {3, 4, 5, 6};
    int delay_t[] = {10, 23, 33, 71};
    int delay_n[] = {20, 9, 6, 3};

    for (i = 0; i < 4; ++i) {
        Create(priorities[i], Client);
        debug("TestTask2: Created task with priority: %d", priorities[i]);
    }

    for (i = 3; i >= 0; --i) {
        Receive(&callee, &msg, sizeof(msg));
        res.t = delay_t[i];
        res.n = delay_n[i];
        Reply(callee, &res, sizeof(res));
    }

    Exit();
}
