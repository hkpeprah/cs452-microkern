/*
 * Generic tests for finish times and create chaining.
 */
#include <utasks.h>
#include <term.h>
#include <stdio.h>
#include <k_syscall.h>
#include <string.h>
#include <kernel.h>
#include <syscall.h>

#define NUM_TASKS  12

static int finishTimes[NUM_TASKS];
static int finishIndex;


void task1() {
    uint32_t tid;
    tid = MyTid();
    finishTimes[finishIndex++] = tid;
    Exit();
}


void task2() {
    uint32_t tid;
    tid = MyTid();
    Pass();
    finishTimes[finishIndex++] = tid;
    Exit();
}


void task3() {
    uint32_t tid;
    tid = MyTid();
    if (tid < NUM_TASKS - 1) {
        Create(1, &task3);
        Pass();
    }
    finishTimes[finishIndex++] = tid;
    Exit();
}


int main() {
    int status;
    void (*code)();
    unsigned int i;
    unsigned int len;
    uint32_t tid;
    int priorities[] = {2, 1, 15, 6, 7, 1, 1, 3, 1};
    int expected[] = {2, 4, 3, 7, 0, 5, 6, 1, 8, 9, 11, 10};

    finishIndex = 0;
    len = 9;
    boot();

    /* test for task creation */
    for (i = 0; i < len; ++i) {
        if (i < 4) {
            code = task2;
        } else if (i >= 4 && i < len - 1) {
            code = task1;
        } else {
            code = task3;
        }

        status = sys_create(priorities[i], code, &tid);

        if (status != 0) {
            printf("Tests Failed: Failed on task %d, could not create tasks.\r\n", i);
            return 1;
        }
    }

    /* test task scheduling */
    kernel_main();

    for (i = 0; i < NUM_TASKS; ++i) {
        if (finishTimes[i] != expected[i]) {
            puts("Test scheduling failed.\r\nExpected:");
            for (i = 0; i < NUM_TASKS; ++i) printf(" %d", expected[i]);
            newline();
            puts("Actual: ");
            for (i = 0; i < NUM_TASKS; ++i) printf(" %d", finishTimes[i]);
            return 1;
        }
    }

    puts("All tests passed successfully!");
    return 0;
}
