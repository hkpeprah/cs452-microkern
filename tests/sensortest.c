#include <k_syscall.h>
#include <kernel.h>
#include <types.h>
#include <term.h>
#include <stdlib.h>
#include <controller.h>
#include <train.h>
#include <stdio.h>
#include <server.h>
#include <random.h>
#include <clock.h>
#include <utasks.h>


void PiggyBack() {
    int status;
    Train_t *train;
    char module1, module2;
    unsigned int id1, id2, i, n;
    char buf[50], *fmt = "%c%u";

    printf("Instructions\r\nPlace train between two sensors you want to check.\r\n"
           "Set train facing the direction you want it to run.\r\n");
    printf("Enter Train Number: ");
    gets(IO, buf, 50);
    n = atoin(buf, &status);
    if (n != 0 && (train = addTrain(n))) {
        printf("\r\nEnter first sensor: ");
        gets(IO, buf, 50);
        if (sscanf(buf, fmt, &module1, &id1)) {
            printf("\r\nEnter second sensor: ");
            gets(IO, buf, 50);
            if (sscanf(buf, fmt, &module2, &id2)) {
                trainSpeed(train->id, 10);
                trainAuxiliary(train->id, TRAIN_LIGHT_OFFSET);
                puts("\r\n");
                for (i = 0; i < 10; i++) {
                    puts("Moving towards Sensor 1\r\n");
                    WaitOnSensor(module1, id1);
                    trainReverse(train->id);
                    puts("Moving towards Sensor 2\r\n");
                    WaitOnSensor(module2, id2);
                    trainReverse(train->id);
                }
                trainSpeed(train->id, 0);
            } else {
                printf("\r\nFailed to parse Sensor 2");
            }
        } else {
            printf("\r\nFailed to parse Sensor 1");
        }
    } else {
        printf("\r\nTrain doesn't exist or not an integer.");
    }
    puts("\r\n");
    SigTerm();
    Exit();
}


int main() {
    unsigned int tid;

    boot();

    sys_create(15, NameServer, &tid);
    sys_create(15, ClockServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(10, TrainController, &tid);
    sys_create(3, PiggyBack, &tid);
    sys_create(13, TimerTask, &tid);

    kernel_main();

    shutdown();
    return 0;
}
