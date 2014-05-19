#include <utasks.h>
#include <term.h>
#include <stdio.h>
#include <k_syscall.h>
#include <string.h>
#include <kernel.h>
#include <syscall.h>

static int finishTimes[8];
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


int main() {
    int status;
    unsigned int i;
    uint32_t tid;
    int priorities[] = {2, 1, 15, 6, 7, 1, 1, 3};
    int expected[] = {2, 4, 3, 7, 0, 5, 6, 1};

    finishIndex = 0;
    boot();

    /* test for task creation */
    for (i = 0; i < 8; ++i) {
        if (i < 4) {
            status = sys_create(priorities[i], task2, &tid);
        } else if (i >= 4) {
            status = sys_create(priorities[i], task1, &tid);
        }

        if (status != 0) {
            printf("Tests Failed: Failed on task %d, could not create tasks.\r\n", i);
            return 1;
        }
    }

    /* test task scheduling */
    kernel_main();

    for (i = 0; i < 8; ++i) {
        if (finishTimes[i] != expected[i]) {
            puts("Test scheduling failed.\r\nExpected:");
            for (i = 0; i < 8; ++i) printf(" %d", expected[i]);
            newline();
            puts("Actual: ");
            for (i = 0; i < 8; ++i) printf(" %d", finishTimes[i]);
            return 1;
        }
    }

    puts("All tests passed successfully!");
    return 0;
}
