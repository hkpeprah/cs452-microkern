/*
 * Generic tests for timer sans-interrupt
 */
#include <kernel.h>
#include <interrupt.h>
#include <syscall.h>
#include <server.h>
#include <clock.h>
#include <term.h>
#include <types.h>
#include <k_syscall.h>

#define NUM_TASKS  4


static int finishTimes[NUM_TASKS];
static int finishIndex = 0;
static int firstClientTid = -1;


typedef struct {
    unsigned int t : 8;
    unsigned int n : 8;
    unsigned int complete : 8;
} DelayMessage;


static void TestClient() {
    unsigned int pTid;
    unsigned int tid;
    unsigned int clock;
    int errno;
    DelayMessage msg;

    pTid = MyParentTid();
    msg.complete = 0;
    errno = Send(pTid, &msg, sizeof(msg), &msg, sizeof(msg));
    tid = MyTid();

    if (errno < 0) {
        error("TestClient: Error: Task %d received status %d sending to %d", tid, errno, pTid);
        Exit();
    }

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

    /* track finish time */
    finishTimes[finishIndex++] = tid;
    printf("Client: Task %d finished.  Calling Exit.\r\n", tid);

    Exit();
}


void Ticker() {
    int callee;
    int timer;
    int errno;
    uint32_t *currentTimer;
    uint32_t tid;
    uint32_t clock;
    unsigned int i;
    DelayMessage msg, res;
    ClockRequest tick;

    tid = Create(15, NameServer);
    clock = Create(15, ClockServer);

    if (tid < 0) {
        printf("Ticker: Error: Could not create NameServer, received %d\r\n", tid);
        Exit();
    } else if (clock < 0) {
        printf("Ticker: Error: Could not craete ClockServer, received %d\r\n", clock);
        Exit();
    }

    for (i = 3; i < 7; ++i) {
        tid = Create(i, TestClient);
        if (tid < 0) {
            printf("Ticker: Error: Could not create TestClient %d\r\n", i - 2);
            firstClientTid = -1;
            Exit();
        } else if (firstClientTid == -1) {
            firstClientTid = tid;
        }
    }

    for (i = 0; i < 4; ++i) {
        Receive(&callee, &msg, sizeof(msg));
        res.t = i + 10;
        res.n = 20 - (i * 4);
        Reply(callee, &res, sizeof(res));
    }

    /* start ticking time */
    timer = 0xFFFFFFFF;
    currentTimer = (uint32_t*)TIMER_VALUE;
    tick.type = TICK;
    for (i = 0; i < 200; ++i) {
        /* 
         * all clients should finish by the time this is done ticking
         */
        while (timer - *currentTimer < 5080);
        timer = *currentTimer;
        errno = Send(clock, &tick, sizeof(tick), &callee, sizeof(callee));
    }

    debug("Ticker: Getting Clock Time.");
    tick.type = TIME;
    Send(clock, &tick, sizeof(tick), &timer, sizeof(timer));
    if (timer != 200) {
        printf("Ticker: Error: Expected time to be 200, received %d\r\n", timer);
        firstClientTid = -1;
    }

    /* should always reach here */
    Exit();
}


int main() {
    int status;
    uint32_t tid, i;

    boot();
    disableInterrupts();

    for (i = 0; i < NUM_TASKS; ++i) {
        finishTimes[i] = -1;
    }
    finishIndex = 0;
    firstClientTid = -1;

    status = sys_create(1, Ticker, &tid);
    if (status < 0) {
        printf("FirstTask: Error: Failed to create task Ticker, received %d", status);
        return 1;
    }

    kernel_main();

    if (firstClientTid < 0) {
        return 1;
    } else {
        for (i = 0; i < NUM_TASKS; ++i) {
            /*
             * finish times should be the order of creation as the
             * clients will preempt the Ticker.
             */
            if (finishTimes[i] != (firstClientTid + (NUM_TASKS - i - 1))) {
                puts("FirstTask: Error: Tests failed.\r\nExpected:\r\n");
                for (i = 0; i < NUM_TASKS; ++i) printf("%d ", firstClientTid + (NUM_TASKS - i - 1));
                puts("\r\nActual:\r\n");
                for (i = 0; i < NUM_TASKS; ++i) printf("%d ", finishTimes[i]);
                puts("\r\n");
                return 2;
            }
        }
    }

    change_color(CYAN);
    puts("All tests passed successfully.\r\n");
    end_color();
    return shutdown();
}
