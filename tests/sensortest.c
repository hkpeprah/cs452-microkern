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

static volatile int RUN = 1;


void PiggyBack() {
    int status;
    TRequest_t msg;
    Train_t *train;
    char module, buf[50], *fmt = "%c%u";
    unsigned int i, n;
    unsigned int sensor1, sensor2, TrainController;

    msg.type = SENSOR_WAIT;
    module = 0;

    printf("Instructions\r\nPlace train between two sensors you want to check.\r\n"
           "Set train facing the direction you want it to run.\r\n");
    printf("Enter Train Number: ");
    gets(IO, buf, 50);
    n = atoin(buf, &status);
    if (n != 0 && (train = addTrain(n))) {
        printf("\r\nEnter first sensor: ");
        gets(IO, buf, 50);
        if (sscanf(buf, fmt, &module, &n)) {
            sensor1 = module - 'A' + n;
            printf("\r\nEnter second sensor: ");
            gets(IO, buf, 50);
            if (sscanf(buf, fmt, &module, &n)) {
                sensor2 = module - 'A' + n;
                TrainController = WhoIs("TrainController");
                trainSpeed(train->id, 10);
                trainAuxiliary(train->id, TRAIN_LIGHT_OFFSET);
                for (i = 0; i < random() % 22; i++) {
                    msg.sensor = &sensor1;
                    Send(TrainController, &msg, sizeof(msg), &status, sizeof(status));
                    if (status < 0) {
                        printf("\r\nInvalid Sensor id: %d\r\nExiting.", sensor1);
                    }
                    trainReverse(train->id);
                    msg.sensor = &sensor2;
                    Send(TrainController, &msg, sizeof(msg), &status, sizeof(status));
                    if (status < 0) {
                        printf("\r\nInvalid Sensor id: %d\r\nExiting.", sensor2);
                    }
                    trainReverse(train->id);
                }
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
    RUN = 0;
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
