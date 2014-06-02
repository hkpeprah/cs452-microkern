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

#define NUM_TASKS  15

static int finishTimes[NUM_TASKS];
static int finishIndex;


void task1() {
    uint32_t tid;
    tid = MyTid();
    finishTimes[finishIndex++] = tid;
}


void task2() {
    uint32_t tid;
    tid = MyTid();
    Pass();
    finishTimes[finishIndex++] = tid;
}


void task3() {
    uint32_t tid;
    tid = MyTid();
    if (tid < NUM_TASKS - 1) {
        Create(2, &task3);
        Pass();
    }
    finishTimes[finishIndex++] = tid;
}


void task4() {
    int digit;
    uint32_t tid;
    unsigned int index;
    char *source;
    char *src[] = {"23304", "48889", "23312"};
    int res[] = {23304, 48889, 23312};
    unsigned int base = 10, num = 0, i = 0;

    tid = MyTid();
    index = tid % 9;
    source = src[index];

    while (source[i]) {
        Pass();
        digit = atod(source[i]);
        if (digit > base || digit < 0) {
            break;
        }
        num = num * base + digit;
        ++i;
    }

    if (num == res[index]) {
        finishTimes[finishIndex++] = tid;
    }
}


int main() {
    int status;
    void (*code)();
    unsigned int i;
    uint32_t tid;
    int priorities[] = {4, 3, 15, 6, 7, 3, 3, 5, 2, 1, 1, 1};
    int expected[] = {2, 4, 3, 7, 0, 5, 6, 1, 8, 12, 14, 13, 9, 10, 11};

    finishIndex = 0;
    boot();

    /* test for task creation */
    for (i = 0; i < 12; ++i) {
        if (i < 4) {
            code = task2;
        } else if (i >= 4 && i < 8) {
            code = task1;
        } else if (i == 8) {
            code = task3;
        } else {
            code = task4;
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
