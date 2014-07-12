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


void ShortMoveTask() {
    char buf[5];
    int status, delay;
    unsigned int tr_number;

    trainSwitch(11, 'C');
    trainSwitch(12, 'C');

    puts("Enter train number: ");
    gets(IO, buf, 5);
    tr_number = atoin(buf, &status);
    printf("Short move testing for train %u\r\n", tr_number);
    if (tr_number >= 0) {
        delay = 10;
        while (true) {
            printf("Train %u with delay %u\r\n", tr_number, delay);
            trainSpeed(tr_number, 10);
            Delay(delay);
            trainSpeed(tr_number, 0);
            if (Continue("Press c to continue: ") == 0) break;
            delay += 20;
        }
    } else {
        printf("Invalid train number: %u\r\n", tr_number);
    }

    printf("Exiting.....");
    shutdown();
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
    sys_create(3, ShortMoveTask, &tid);

    kernel_main();

    shutdown();
    return 0;
}
