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


void testTask() {
    printf("Calling task with priority: %d\r\n", getCurrentTask()->priority);
    Exit();
}


static void Client() {
    unsigned int pTid;
    unsigned int tid;
    unsigned int clock;
    int errno;
    DelayMessage msg;

    pTid = MyParentTid();
    msg.complete = 0;
    errno = Send(pTid, &msg, sizeof(msg), &msg, sizeof(msg));
    tid = MyTid();

    if (errno >= 0) {
        /*
         * Client delays n times, each for a time interval of t, printing after
         * each before exiting.
         */
        clock = WhoIs(CLOCK_SERVER);
        debugf("Client: Task %d with (Interval %d, Number of Delays %d).", tid, msg.t, msg.n);
        while (msg.complete < msg.n) {
            Delay(msg.t);
            printf("Tid: %d\tInterval: %d\tComplete: %d\r\n", tid, msg.t, ++msg.complete);
        }
    } else {
        error("Client: Error: Task %d received status %d sending to %d", tid, errno, pTid);
    }

    Exit();
}


void firstTask() {
    int callee;
    unsigned int i;
    DelayMessage msg, res;
    int priorities[] = {3, 4, 5, 6};
    int delay_t[] = {10, 23, 33, 71};
    int delay_n[] = {20, 9, 6, 3};

    /*
     * First user task creats the ClockServer and four
     * client tasks.
     */
    Create(15, NameServer);
    Create(15, ClockServer);

    for (i = 0; i < 4; ++i) {
        Create(priorities[i], Client);
        debugf("FirstTask: Created task with priority: %d", priorities[i]);
    }

    for (i = 0; i < 4; ++i) {
        Receive(&callee, &msg, sizeof(msg));
        res.t = delay_t[i];
        res.n = delay_n[i];
        Reply(callee, &res, sizeof(res));
    }

    /* push the shell as the last task */
    Create(0, Shell);

    debug("FirstTask: Exiting");

    Exit();
}
