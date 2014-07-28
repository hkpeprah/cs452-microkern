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
#include <stdlib.h>

#define TIMER_LOAD     0x80810080
#define TIMER_VALUE    0x80810084
#define TIMER_ENABLE   0x00000080
#define TIMER_MODE     0x00000040
#define TIMER_508KHZ   0x00000008

#define CLOCK_SERVER   "ClockServer"

static int clockserver_tid = -1;

struct DelayQueue_t;


typedef enum {
    DELAY = 0,
    DELAY_UNTIL,
    TIME,
    TICK
} ClockRequestType;


typedef struct {
    short type;
    unsigned int ticks;
} ClockRequest;


typedef struct DelayQueue_t {
    uint32_t tid;
    uint32_t delay;
    struct DelayQueue_t *next;
} DelayQueue;


static void ClockNotifier() {
    /*
     * Awaits an interrupt event corresponding to a tick.  On each tick, notifies
     * the ClockServer.
     */
    int n;
    int errno;
    unsigned int clock;
    ClockRequest msg;

    clock = MyParentTid();
    msg.type = TICK;
    while ((n = AwaitEvent(CLOCK_INTERRUPT, NULL, 0))) {
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
    uint32_t notifier;
    ClockRequest msg;
    DelayQueue __bank[32];
    DelayQueue *free, *queue, *tmp, *last, *node;

    free = NULL;
    queue = NULL;
    clockserver_tid = MyTid();
    notifier = Create(15, ClockNotifier);
    for (i = 0; i < 32; ++i) {
        __bank[i].next = free;
        free = &__bank[i];
    }

    errno = RegisterAs(CLOCK_SERVER);
    if (errno < 0) {
        error("ClockServer: Error: NameServer returned %d", errno);
    }

    *((uint32_t*)TIMER_LOAD) = 5080; /* one millisecond */
    *((uint32_t*)TIMER_CONTROL) = TIMER_ENABLE | TIMER_508KHZ | TIMER_MODE;

    ticks = 0;
    while (true) {
        errno = Receive(&callee, &msg, sizeof(msg));
        if (errno < 0 || errno != sizeof(msg)) {
            switch(errno) {
                case NO_AVAILABLE_MESSAGES:
                    error("ClockServer: Error: No available messages.");
                    break;
                default:
                    error("ClockServer: Error: Message length mistmatch: %d == %d", errno, sizeof(msg));
            }
            continue;
        }

        switch (msg.type) {
            /* 
             * In the case of Delay or DelayUntil, what we return does not
             * actually matter.
             */
            case TICK:
                if (callee != notifier) {
                    continue;
                } else {
                    /* need to immediately reply to the ClockNotification task to unblock */
                    errno = 0;
                    Reply(callee, &errno, sizeof(errno));
                }
                ++ticks;
                tmp = queue;
                last = NULL;
                while (queue != NULL && queue->delay <= ticks) {
                    Reply(queue->tid, &ticks, sizeof(ticks));
                    last = queue;
                    queue = queue->next;
                }
                if (last != NULL) {
                    last->next = free;
                    free = tmp;
                }
                break;
            case DELAY:
                msg.ticks += ticks;
            case DELAY_UNTIL:
                if (msg.ticks <= ticks) {
                    Reply(callee, &ticks, sizeof(ticks));
                } else if (free == NULL) {
                    errno = OUT_OF_SPACE;
                    Reply(callee, &errno, sizeof(errno));
                } else {
                    node = free;
                    free = free->next;
                    node->tid = callee;
                    node->delay = msg.ticks;

                    if (queue == NULL) {
                        node->next = queue;
                        queue = node;
                    } else {
                        tmp = queue;
                        last = NULL;
                        while (tmp != NULL && tmp->delay <= node->delay) {
                            last = tmp;
                            tmp = tmp->next;
                        }

                        if (last == NULL) {
                            node->next = queue;
                            queue = node;
                        } else {
                            last->next = node;
                            node->next = tmp;
                        }
                    }
                }
                break;
            case TIME:
                errno = ticks;
                Reply(callee, &errno, sizeof(errno));
                break;
            default:
                error("ClockServer: Error: Received unknown request %d from Task %d", msg.type, callee);
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
    errno = Send(clockserver_tid, &msg, sizeof(msg), &ticks, sizeof(ticks));
    if (errno < 0) {
        error("Delay: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    if (ticks < 0) {
        error("Delay: No more slots for delaying, returning to %d\r\n", MyTid());
        return -3;
    }

    return ticks;
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
    errno = Send(clockserver_tid, &msg, sizeof(msg), &ticks, sizeof(ticks));
    if (errno < 0) {
        error("DelayUntil: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, clockserver_tid);
        return -2;
    }

    if (ticks < 0) {
        error("DelayUntil: No more slots for delaying, returning to %d\r\n", MyTid());
        return -3;
    }

    return ticks;
}


static void DelayCourier() {
    int parent = MyParentTid();
    int ticks, callee, status;

    // recv/reply immediately
    status = Receive(&callee, &ticks, sizeof(ticks));
    Reply(callee, NULL, 0);

    if (status < 0 || callee != parent) {
        error ("DelayCourier: error recv from %d with status %d", callee, status); 
    }

    ASSERT((status = Delay(ticks)) >= 0, "Delay returned %d", status);
    Send(parent, NULL, 0, NULL, 0);
}


int CourierDelay(int ticks, int priority) {
    int courier, result;

    courier = Create(priority, DelayCourier);
    if (courier < 0) {
        return courier;
    }

    result = Send(courier, &ticks, sizeof(ticks), NULL, 0);
    if (result < 0) {
        error ("CourierDelay: error in send %d to child %d", result, courier);
        return result;
    }
    return courier;
}
