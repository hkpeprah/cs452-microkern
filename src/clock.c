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
#include <util.h>


static int clockserver_tid = -1;


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
    error("ClockNotifier: Error: Exited.");
    Exit();
}


void ClockServer() {
    int ticks;
    int errno;
    int callee;
    unsigned int i;
    ClockRequest msg;
    DelayQueue __bank[32];
    DelayQueue *free, *queue, *tmp, *last;

    free = NULL;
    queue = NULL;
    clockserver_tid = MyTid();
    Create(15, ClockNotifier);
    for (i = 0; i < 32; ++i) {
        __bank[i].next = free;
        free = &__bank[i];
    }

    errno = RegisterAs(CLOCK_SERVER);
    if (errno < 0) {
        error("ClockServer: Error: NameServer returned %d", errno);
    }

    /*
     * 1 kHz = 0.001 seconds => 1 kHz = 1 millisecond
     * 508 ticks/ms, so tenth of second (100 milliseconds) = 508 * 100
     */
    *((uint32_t*)TIMER_CONTROL) = TIMER_ENABLE | TIMER_508KHZ;
    *((uint32_t*)TIMER_LOAD) = 50800;

    ticks = 0;
    while (true) {
        errno = Receive(&callee, &msg, sizeof(msg));
        if (errno != sizeof(msg)) {
            error("ClockServer: Error: Message length mistmatch %d != %d", errno, sizeof(msg));
        }

        switch (msg.type) {
            /* 
             * In the case of Delay or DelayUntil, what we return does not
             * actually matter.
             */
            case TICK:
                ++ticks;
                /* need to immediately reply to the ClockNotification task to unblock */
                errno = 0;
                Reply(callee, &errno, sizeof(errno));
                errno = ticks;
                while (queue != NULL && queue->delay <= ticks) {
                    debugf("ClockServer: Waking up Task %d", queue->tid);
                    Reply(queue->tid, &errno, sizeof(errno));
                    tmp = queue->next;
                    queue->next = free;
                    free = queue;
                    queue = tmp;
                }
                break;
            case DELAY:
                msg.ticks += ticks;
            case DELAY_UNTIL:
                if (msg.ticks <= ticks) {
                    errno = 0;
                    Reply(callee, &errno, sizeof(errno));
                } else if (free == NULL) {
                    errno = OUT_OF_SPACE;
                    Reply(callee, &errno, sizeof(errno));
                } else {
                    free->tid = callee;
                    free->delay = msg.ticks;
                    /* insert sorted into linked list */
                    if (queue == NULL) {
                        tmp = free->next;
                        free->next = queue;
                        queue = free;
                        free = tmp;
                    } else {
                        tmp = queue;
                        while (tmp != NULL && free->delay >= tmp->delay) {
                            last = tmp;
                            tmp = tmp->next;
                        }

                        if (tmp == NULL) {
                            /* add callee as tail, has largest delay */
                            last->next = free;
                            free = free->next;
                            last->next->next = NULL;
                        } else {
                            /* swap callee with the current tmp */
                            last = free;
                            free = free->next;
                            last->next = tmp->next;
                            tmp->next = last;
                            last->tid = tmp->tid;
                            tmp->tid = callee;
                            callee = tmp->delay;
                            tmp->delay = last->delay;
                            last->delay = callee;
                        }
                    }
                }
                break;
            case TIME:
                errno = ticks;
                Reply(callee, &errno, sizeof(errno));
                break;
            default:
                errno = -1;
                Reply(callee, &errno, sizeof(errno));
        }
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
        error("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
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
        error("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
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
        error("Time: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    return 0;
}
