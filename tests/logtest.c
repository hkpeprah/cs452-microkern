#include <term.h>
#include <util.h>
#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <server.h>
#include <ts7200.h>
#include <syscall.h>
#include <k_syscall.h>
#include <logger.h>
#include <clock.h>

static int RUN = 1;

void looper() {
    char res;

    while (1) {
        // debug("Looper: Waiting on character");
        res = Getc(COM2);

        if(res == 'q') {
            RUN = 0;
            break;
        }

        Putc(COM2, res);
    }
}


void null() {
    while (RUN) {
        Pass();
    }
}


int main() {
    unsigned int tid;

    boot();

    sys_create(15, NameServer, &tid);
    sys_create(10, InputServer, &tid);
    sys_create(10, OutputServer, &tid);
    sys_create(1, looper, &tid);
    sys_create(0, null, &tid);

    kernel_main();

    dumpLog();

    return 0;
}
