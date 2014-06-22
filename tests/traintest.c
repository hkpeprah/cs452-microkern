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
#include <stdio.h>
#include <term.h>

static int RUN = 1;

void looper() {
    char res;
    char sensor[11];
    char s[] = { 10, 49 };
    char t[] = { 0, 49};
    int i;

    while (1) {
        // debug("Looper: Waiting on character");
        res = Getc(COM2);

        if(res == 'q') {
            RUN = 0;
            break;
        } else if (res == 'p') {
            //res = Putc(COM1, 133);
            res = Putc(COM1, 133);
            printf("sensor returned: %d\n", res);
            Getcn(COM1, sensor, 10);

            for (i = 0; i < 10; ++i) {
                printf("0x%x ", sensor[i]);
            }
            printf("\n");

        } else if (res == 's') {
            Putcn(COM1, s, 2);
        } else if (res == 't') {
            Putcn(COM1, t, 2);
        } else {
            Putc(COM2, res);
        }
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
