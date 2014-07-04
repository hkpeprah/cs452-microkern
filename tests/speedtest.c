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


void SpeedTest() {
    int status;
    char buf[50];
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
        printf("Speed Calculations for Train %d\r\n", tr_number);
        tr_number = TrCreate(6, tr_number, NearestSensorEdge('B', 9));
        if (tr_number >= 0) {
            for (speed = 3; speed <= TRAIN_MAX_SPEED; ++speed) {
                TrSpeed(tr_number, speed);
                status = WaitAnySensor();
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
            TrSpeed(tr_number, speed);
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

    kernel_main();

    shutdown();

    return 0;
}
