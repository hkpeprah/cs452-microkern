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
#include <utasks.h>
#include <train.h>


void Fig8() {
    // start train

    while (true) {
        // wait till e16 triggers
        // flip switch: 153 = s, 154 = c
        // wait till e3 triggers
        // flip switch: 153 = c, 154 = s
    }
}


int main() {
    int tid;

    boot();

    sys_create(15, NameServer, &tid);
    sys_create(15, ClockServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(5, TrainUserTask, &tid);
    sys_create(13, TimerTask, &tid);
    sys_create(1, Fig8, &tid);

    kernel_main();

    shutdown();
    return 0;
}
