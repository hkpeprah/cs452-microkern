/*
 * clock.c - Clock server and utilities
 * Notes:
 *    - In CS452 this term, ticks are 10 milliseconds
 */
#include <clock.h>
#include <term.h>
#include <syscall.h>
#include <server.h>
#include <syscall_types.h>


int clockserver_tid = -1;


static void ClockNotifier() {
    /*
     * Awaits an interrupt event corresponding to a tick.  On each tick, notifies
     * the ClockServer.
     */
    int n;
    int errno;
    unsigned int clock;
    ClockRequest msg;

    while ((n = AwaitEvent(EVENT_CLOCK))) {
        msg.type = TICK;
        clock = MyParentTid();
        errno = Send(clock, &msg, sizeof(msg), &n, sizeof(n));
    }

    /* should never reach here */
    Exit();
}


void ClockServer() {
    int ticks;
    int errno;
    int callee;
    int response;
    ClockRequest msg;
    unsigned int tid;

    clockserver_tid = MyTid();
    tid = Create(1, ClockNotifier);

    errno = RegisterAs(CLOCK_SERVER);
    if (errno < 0) {
        debugf("Error: ClockServer: NameServer returned %d", errno);
    }

    ticks = 0;
    while (Receive(&callee, &msg, sizeof(msg))) {
        switch (msg.type) {
            /* 
             * In the case of Delay or DelayUntil, what we return does not
             * actually matter.
             */
            case TICK:
                ++ticks;
                response = 0;
                break;
            case DELAY:
                response = 0;
                break;
            case TIME:
                response = ticks;
                break;
            case DELAY_UNTIL:
                response = 0;
                break;
            default:
                response = -1; /* unknown request */
        }
        Reply(callee, &response, sizeof(response));
    }

    /* should never reach here */
    Exit();
}


int Time() {
    /*
     * Time is a wrapper for a Send to the ClockServer.
     * Returns the number of ticks since the clock server was initialized.
     * -1 if wrapper invalid, -2 if tid is not that of ClockServer
     */
    int errno;
    int ticks;
    ClockRequest msg;

    if (clockserver_tid < 0) {
        return -1;
    }

    msg.type = TIME;
    errno = Send(clockserver_tid, &msg, sizeof(msg), &ticks, sizeof(ticks));
    if (errno < 0) {
        debugf("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    return ticks;
}


int Delay(int ticks) {
    /*
     * Delay returns after the given number of ticks has passed.
     * 0 on success, -1 if wrapper tid is not right, -2 if wrapper tid is not clock server
     */
    int errno;
    ClockRequest msg;

    if (clockserver_tid < 0) {
        return -1;
    }

    msg.type = DELAY;
    msg.ticks = ticks;
    errno = Send(clockserver_tid, &msg, sizeof(msg), &msg, sizeof(msg));
    if (errno < 0) {
        debugf("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    return 0;
}


int DelayUntil(int ticks) {
    /*
     * DelayUntil returns when time since clock initialization is greater than ticks.
     * 0 on success, -1 if wrapper tid is not right, -2 if wrapper tid is not clock server
     */
    int errno;
    ClockRequest msg;

    if (clockserver_tid < 0) {
        return -1;
    }

    msg.type = DELAY_UNTIL;
    msg.ticks = ticks;
    errno = Send(clockserver_tid, &msg, sizeof(msg), &msg, sizeof(msg));
    if (errno < 0) {
        debugf("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    return 0;
}
