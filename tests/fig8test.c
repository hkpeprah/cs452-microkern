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
#include <controller.h>


void Fig8() {
    int result;
    int tr;
    char buf[4];

    printf("Enter Train Number: ");
    gets(IO, buf, 4);
    tr = atoin(buf, &result);

    trainSwitch(14, 'c');
    addTrain(tr);
    trainSpeed(tr, 8);
    
    while (true) {
        result = WaitOnSensor('E', 16);
        if (result == 1) {
            printf("got e16\n");
            trainSwitch(153, 's');
            trainSwitch(154, 'c');
        } else {
            printf("got %d\n", result);
        }

        result = WaitOnSensor('B', 14);
        if (result == 1) {
            printf("got b14\n");
        } else {
            printf("got %d\n", result);
        }

        result = WaitOnSensor('E', 3);
        if (result == 1) {
            printf("got e3\n");
            trainSwitch(153, 'c');
            trainSwitch(154, 's');
        } else {
            printf("got %d\n", result);
        }

        result = WaitOnSensor('C', 1);
        if (result == 1) {
            printf("got c1\n");
        } else {
            printf("got %d\n", result);
        }
    }
}


int main() {
    unsigned int tid;

    boot();

    sys_create(15, NameServer, &tid);
    sys_create(15, ClockServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(5, TrainUserTask, &tid);
    sys_create(13, TimerTask, &tid);
    sys_create(1, Fig8, &tid);
    sys_create(10, TrainController, &tid);

    kernel_main();

    shutdown();
    return 0;
}
