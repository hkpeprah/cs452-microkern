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

static int RUN = 1;


void looper() {
    char res;

    for (;;) {
        printf("Looper: Waiting on character: ");
        // debug("Looper: Waiting on character");
        res = Getc(COM2);
        Putc(COM2, res);
        Putc(COM2, '\r');
        Putc(COM2, '\n');
    }
}


void null() {
    while (RUN);
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

    return 0;
}
