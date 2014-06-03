#include <utasks.h>
#include <perf_test.h>
#include <term.h>
#include <stdio.h>
#include <k_syscall.h>
#include <kernel.h>

#define FIRST_PRIORITY     14
#include <ts7200.h>
#include <syscall.h>
#include <shell.h>

void interruptTestTask() {
    int i = 0;
    char c;
    do {
        printf("%d\n", i++);
        c = getchar();

        if(c == 'i') {
            // trigger interrupt here!
        }

    } while(c != 'q');

    Exit();
}

int main() {
    int status;
    uint32_t tid;

    boot();

    /* create the first user task */
    #if PROFILE
        initPerf();
        status = sys_create(FIRST_PRIORITY, perfGenesisTask, &tid);
    #else
        status = sys_create(FIRST_PRIORITY, firstTask, &tid);
    #endif

    if (status != 0) {
        /* something went wrong creating the first user task */
        return -1;
    }
    
    kernel_main();

    // should reach here after all work has been done
    return shutdown();
}
