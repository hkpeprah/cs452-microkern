/*
 * utasks.c - User tasks for second part of kernel
 * First Task
 *    - Bootstraps the other tasks and the clients.
 *    - Quits when it has gone through all the clients.
 *
 * Client
 *    - Created by first user task, calls parent to get delay time t, and number of
 *      delays, n.
 *    - Uses WhoIs to discover Clock Server.
 *    - Delays n times, each for time interval, t, and prints after each.
 */
#include <term.h>
#include <utasks.h>
#include <syscall.h>
#include <shell.h>
#include <train.h>
#include <uart.h>
#include <stdlib.h>


void firstTask() {
    int id;
    id = Create(15, NameServer);
    id = Create(15, ClockServer);
    id = Create(12, InputServer);
    id = Create(12, OutputServer);
    id = Create(1, Shell);
    id = Create(5, TrainUserTask);
    id = Create(13, TimerTask);

    debug("FirstTask: Exiting.");
    Exit();
}


void testTask() {
    printf("Calling task with priority: %d\r\n", getCurrentTask()->priority);
    Exit();
}


static void Client() {
    unsigned int pTid;
    unsigned int tid;
    int errno;
    DelayMessage msg, res;

    pTid = MyParentTid();
    msg.complete = 0;
    errno = Send(pTid, &msg, sizeof(msg), &res, sizeof(res));
    tid = MyTid();

    if (errno >= 0) {
        /*
         * Client delays n times, each for a time interval of t, printing after
         * each before exiting.
         */
        debug("Client: Task %d with (Interval %d, Number of Delays %d).", tid, res.t, res.n);
        while (res.n--) {
            Delay(res.t);
            res.complete += 1;
            printf("Tid: %d      Delay Interval: %d     Delays Complete: %d\r\n", tid, res.t, res.complete);
        }
    } else {
        error("Client: Error: Task %d received status %d sending to %d", tid, errno, pTid);
    }

    Exit();
}


void testTask2() {
    int i;
    int callee;
    DelayMessage msg, res;
    int priorities[] = {3, 4, 5, 6};
    int delay_t[] = {10, 23, 33, 71};
    int delay_n[] = {20, 9, 6, 3};

    for (i = 0; i < 4; ++i) {
        Create(priorities[i], Client);
        debug("TestTask2: Created task with priority: %d", priorities[i]);
    }

    for (i = 3; i >= 0; --i) {
        Receive(&callee, &msg, sizeof(msg));
        res.t = delay_t[i];
        res.n = delay_n[i];
        Reply(callee, &res, sizeof(res));
    }

    Exit();
}


void TimerTask() {
    unsigned int count;
    unsigned int cpuUsage;

    while (true) {
        Delay(10);
        count = Time();
        cpuUsage = CpuIdle();
        updateTime(count, cpuUsage);
    }

    Exit();
}


static void TrainSensorSlave() {
    TrainMessage t;
    bool bit;
    int status, byte1, byte2, args[2];
    unsigned int i, j, parent;
    volatile bool sensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    parent = MyParentTid();

    debug("TrainSensorSlave: Tid %d", MyTid());
    t.args = args;
    args[0] = TRAIN_POLL_SENSORS;
    args[1] = (int)sensors;

    while (true) {
        pollSensors();
        for (i = 0; i < TRAIN_MODULE_COUNT; ++i) {
            byte1 = trgetchar();
            byte2 = trgetchar();
            for (j = 0; j < TRAIN_SENSOR_COUNT; ++j) {
                if (j < TRAIN_SENSOR_COUNT / 2) {
                    /* first byte corresponds to sensors 1 - 8 */
                    bit = EXTRACT_BIT(byte1, (TRAIN_SENSOR_COUNT / 2) - j);
                } else {
                    /* lower byte corresponds to sensors 8 - 16 */
                    bit = EXTRACT_BIT(byte2, TRAIN_SENSOR_COUNT - j);
                }
                sensors[(i * TRAIN_SENSOR_COUNT) + j] = (bool)(bit & 1);
            }
        }
        Send(parent, &t, sizeof(t), &status, sizeof(status));
        Delay(50);
        resetSensors();
    }

    Exit();
}


void TrainUserTask() {
    Train_t *train;
    TrainMessage t;
    int status, speed;
    int cmd, callee, bytes;
    bool sigkill, *sensors;
    unsigned int i, courier;

    RegisterAs("TrainHandler");
    courier = Create(6, TrainSensorSlave);
    sensors = NULL;
    sigkill = false;

    while (sigkill == false) {
        bytes = Receive(&callee, &t, sizeof(t));
        cmd = t.args[0];
        /* switches on the command and validates it */
        status = 0;
        switch (cmd) {
            case TRAIN_GET_SENSOR:
                if (sensors == NULL) {
                    status = -3;
                } else {
                    if (t.args[1] >= 'A' && t.args[1] <= 'A' + TRAIN_MODULE_COUNT - 1) {
                        if (t.args[2] >= 0 && t.args[2] <= TRAIN_SENSOR_COUNT) {
                            status = sensors[t.args[1] - 'A' + t.args[2]];
                        } else {
                            status = -2;
                        }
                    } else {
                        status = -1;
                    }
                }
                break;
            case TRAIN_POLL_SENSORS:
                /* new sensor data */
                if (callee == courier) {
                    /* validate callee actually child */
                    sensors = (bool*)t.args[1];
                    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
                        if (sensors[i]) {
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT));          // index in module
                        }
                    }
                }
                break;
            case TRAIN_GO:
                turnOnTrainSet();
                debug("Starting Train Controller");
                break;
            case TRAIN_STOP:
                turnOffTrainSet();
                debug("Stopping Train Controller");
                sigkill = true;
                break;
            case TRAIN_SPEED:
                status = trainSpeed((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status == 0) {
                    debug("Setting Train %u at Speed %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or speed.\r\n");
                }
                break;
            case TRAIN_SWITCH:
                status = trainSwitch((unsigned int)t.args[1], (int)t.args[2]);
                if (status == 0) {
                    printSwitch((unsigned int)t.args[1], (char)t.args[2]);
                    debug("Toggling Switch %u to State %c", t.args[1], toUpperCase(t.args[2]));
                    Delay(30);
                    turnOffSolenoid();
                } else {
                    printf("Error: Invalid state or switch.\r\n");
                }
                break;
            case TRAIN_AUX:
                status = trainAuxiliary((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status == 0) {
                    debug("Toggling auxiliary function %u for Train %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or auxiliary function.\r\n");
                }
                break;
            case TRAIN_RV:
                if ((train = getTrain((unsigned int)t.args[1]))) {
                    debug("Reversing train: %u", t.args[1]);
                    speed = train->speed;
                    trainSpeed(train->id, 0);
                    Delay(speed + 30);
                    trainReverse(train->id);
                    Delay(speed + 30);
                    trainSpeed(train->id, speed);
                    status = 0;
                } else {
                    printf("Error: Invalid train.\r\n");
                    status = -1;
                }
                break;
            case TRAIN_LI:
                status = trainAuxiliary((unsigned int)t.args[1], TRAIN_LIGHT_OFFSET);
                if (status == 0) {
                    debug("Turning on/off the lights on train: %u", t.args[1]);
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_HORN:
                status = trainAuxiliary((unsigned int)t.args[1], TRAIN_HORN_OFFSET);
                if (status == 0) {
                    debug("Turning on/off horn on train: %u", t.args[1]);
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_ADD:
                if (addTrain((unsigned int)t.args[1])) {
                    debug("Added Train %u to Controller.", t.args[1]);
                }
                break;
        }
        Reply(callee, &status, sizeof(status));
    }

    Exit();
}
