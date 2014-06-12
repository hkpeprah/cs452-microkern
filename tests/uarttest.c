#include <term.h>
#include <util.h>
#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <ts7200.h>
#include <syscall.h>
#include <k_syscall.h>

void looper() {
    int *vicbase = (int*) VIC2_BASE;
    int res;

    for (;;) {
//        printf("fuck\n");
//        vicbase[VICxSoftInt] = 1 << 22;
        res = AwaitEvent(UART2_RCV_INTERRUPT, NULL, 0);
        printf("got: %c", res);
    }
}

void null() {

    for (;;) {
    }
}

int main() {
    int tid;

    boot();
    enableUartInterrupts();

    sys_create(1, looper, &tid);
    sys_create(0, null, &tid);

    kernel_main();


    return 0;
}
