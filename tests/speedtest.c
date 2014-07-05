#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <server.h>
#include <syscall.h>
#include <logger.h>
#include <clock.h>
#include <stdio.h>
#include <term.h>
#include <controller.h>
#include <k_syscall.h>
#include <stdlib.h>
#include <utasks.h>
#include <string.h>
#include <train_task.h>
#include <sensor_server.h>


static unsigned int Continue() {
    char ch;
    printf("Press c to continue: ");
    ch = getchar();
    printf("%c\r\n", ch);
    if (ch == 'c') return 1;
    return 0;
}


void SpeedTest() {
    int status;
    char ch, buf[50];
    Sensor_t *sensor;
    unsigned int speed, tr_number, startTime, endTime;

    Delay(10);
    trainSwitch(10, 'S');
    trainSwitch(13, 'S');
    trainSwitch(14, 'C');
    trainSwitch(17, 'S');
    trainSwitch(16, 'S');

    printf("Instructions:\r\n");
    while (true) {
        printf("Enter Train Number (or q to quit): ");
        gets(IO, buf, 50);
        if (strcmp(buf, "q") == 0) {
            break;
        }
        tr_number = atoin(buf, &status);
        printf("Speed Calculations for Train %u\r\n", tr_number);
        tr_number = TrCreate(6, tr_number, NearestSensorEdge('B', 9));
        if (tr_number >= 0) {
            TrSpeed(tr_number, 0);
            for (speed = 3; speed <= TRAIN_MAX_SPEED; ++speed) {
                printf("Speed or acceleration test? (s|a|c) ");
                ch = getchar();
                printf("%c\r\n", ch);
                TrSpeed(tr_number, speed);
                if (ch == 'a' || ch == 'A') {
                    printf("Acceleration/Deceleration for speed %u\r\n", speed);
                    WaitOnSensor('D', 13);
                    printf("Stopping beginning at sensor D13.\r\n");
                    WaitOnSensor('D', 13);
                    TrSpeed(tr_number, 0);
                } else if (ch == 'c' || ch == 'C') {
                    printf("Skipping speed %u\r\n", speed);
                    continue;
                } else {
                    printf("Measuring for speed %u\r\n", speed);
                    status = WaitAnySensor();
                    if (status == 0) {
                        continue;
                    }
                    sensor = getSensorFromIndex(status);
                    if (sensor == NULL) {
                        break;
                    }
                    printf("Using Sensor: %c%u\r\n", sensor->module, sensor->id);
                    WaitOnSensor(sensor->module, sensor->id);
                    startTime = Time();
                    WaitOnSensor(sensor->module, sensor->id);
                    endTime = Time();
                    printf("Speed %d, Time %d\r\n", speed, endTime - startTime);
                }
                if (!Continue()) {
                    break;
                }
            }
        }
    }

    turnOffTrainSet();
    Delay(10);
    SigTerm();
    Exit();
}


int main() {
    unsigned int tid;

    boot();

    sys_create(15, ClockServer, &tid);
    sys_create(15, NameServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(11, TrainController, &tid);
    sys_create(13, TimerTask, &tid);
    sys_create(4, SpeedTest, &tid);
    sys_create(12, SensorServer, &tid);

    kernel_main();

    shutdown();

    return 0;
}
