#include <kernel.h>
#include <train_speed.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>
#include <k_syscall.h>
#include <syscall.h>
#include <term.h>
#include <server.h>
#include <uart.h>
#include <sensor_server.h>
#include <utasks.h>
#include <train.h>


static unsigned int Continue(char *msg) {
    char ch;
    puts(msg);
    ch = getchar();
    printf("%c\r\n", ch);
    if (ch == 'c') return 1;
    return 0;
}


void ShortRevTask() {
    char buf[5];
    int status, delay;
    unsigned int tr_number;

    trainSwitch(11, 'C');
    trainSwitch(12, 'C');

    while (true) {
        puts("Enter train number: ");
        gets(IO, buf, 5);
        tr_number = atoin(buf, &status);
        printf("Short move testing for train %u\r\n", tr_number);
        delay = 140;
        while (true) {
            if (tr_number >= 0) {
                printf("Delay: %u\r\n", delay);
                trainSpeed(tr_number, 10);
                Delay(delay);
                trainSpeed(tr_number, 0);
                Delay(getTransitionTicks(tr_number, 10, 0));
                trainReverse(tr_number);
                trainSpeed(tr_number, 10);
                if (!Continue("Press c to stop train: ")) break;
            } else {
                puts("Invalid train number.");
            }
            delay += 20;
            trainSpeed(tr_number, 0);
            trainReverse(tr_number);
            if (!Continue("Press c to move to next delay speed: ")) break;
        }
        if (!Continue("Press c to try with next train: ")) break;
    }

    Exit();
}


int main () {
    unsigned int tid;

    boot();

    sys_create(15, ClockServer, &tid);
    sys_create(15, NameServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(13, TimerTask, &tid);
    sys_create(12, SensorServer, &tid);
    sys_create(3, ShortRevTask, &tid);

    kernel_main();

    shutdown();
    return 0;
}
