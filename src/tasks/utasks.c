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
    id = Create(14, InputServer);
    id = Create(14, OutputServer);
    id = Create(0, NullTask);
    id = Create(1, Shell);
    id = Create(5, TrainUserTask);

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


void timerTask() {
    unsigned int count;

    save_cursor();
    move_cursor(0, 0);
    change_color(GREEN);
    printf("Time: ");
    end_color();
    restore_cursor();

    while (true) {
        Delay(1);
        count = Time();
        save_cursor();
        move_cursor(7, 0);
        printf("%d:%d:%d", count / 6000, (count / 100) % 60, count % 100);
        restore_cursor();
    }

    Exit();
}


void TrainSlave() {
    unsigned int ut, speed;
    unsigned int id;
    TrainMessage msg;
    int type, status, callee, bytes;
    Train_t *train;

    ut = MyParentTid();
    status = 1;

    while (true) {
        bytes = Receive(&callee, &msg, sizeof(msg));
        if (bytes < 0) {
            error("TrainUserCourier: Got send %d from Task %d", bytes, callee);
        } else {
            type = msg.args[0];
            id = msg.args[1];
            if (ut == callee && bytes > 0) {
                switch (type) {
                    case TRAIN_RV:
                        if ((train = getTrain(id))) {
                            speed = train->speed;
                            trainSpeed(train->id, 0);           /* deramp train speed */
                            Delay(speed + 30);                  /* pulling this number out our asses */
                            trainReverse(train->id);
                            Delay(speed + 30);
                            trainSpeed(train->id, speed);       /* ramp up again */
                            debug("Train reverse completed.");
                        }
                        break;
                    case TRAIN_SWITCH:
                        Delay(30);                          /* solenoid must be turned off after atleast 150ms have passed */
                        turnOffSolenoid();
                        debug("Solenoid turned off.");
                        break;
                }
            }
        }
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}


void TrainSensorSlave() {
    TrainMessage t;
    int byte1, byte2, callee;
    unsigned int i, j, parent;
    bool bit;
    bool sensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT];
    int args[2];

    parent = MyParentTid();

    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
        sensors[i] = (bool)0;
    }

    debug("TrainSensorSlave: Tid %d", MyTid());
    Receive(&callee, &t, sizeof(t));
    t.args = args;
    args[0] = TRAIN_POLL_SENSORS;
    args[1] = (int)&sensors;

    if (callee != parent) {
        error("Error: TrainSensorSlave: Received request not from parent.");
        Exit();
    } else {
        Reply(parent, &t, sizeof(t));
    }

    while (true) {
        pollSensors();
        debug("TrainSensorSlave: Polled sensors.");
        for (i = 0; i < TRAIN_MODULE_COUNT; ++i) {
            byte1 = trgetchar();
            debug("Byte 1: %c\r\n", byte1);
            byte2 = trgetchar();
            debug("Byte 2: %c\r\n", byte1);
            for (j = 0; j < TRAIN_SENSOR_COUNT; ++j) {
                if (i < TRAIN_SENSOR_COUNT / 2) {
                    /* first byte corresponds to sensors 1 - 8 */
                    bit = EXTRACT_BIT(byte1, TRAIN_SENSOR_COUNT / 2 - i);
                } else {
                    /* lower byte corresponds to sensors 8 - 16 */
                    bit = EXTRACT_BIT(byte2, TRAIN_SENSOR_COUNT - i);
                }
                sensors[i + j] = (bool)(bit & 1);
            }
        }
        debug("TrainSensorSlave: Sending updated sensor data to Parent Task %d.", parent);
        Send(parent, &t, sizeof(i), &i, sizeof(i));
    }

    Exit();
}


void TrainUserTask() {
    TrainMessage t, msg;
    int cmd, callee, bytes;
    int status1, status2;
    int argument_buf[3];
    unsigned int i;
    unsigned int courier, sensorCourier;
    bool *sensors;
    bool sigkill;

    RegisterAs("TrainHandler");
    turnOnTrainSet();
    clearTrainSet();
    displayInfo();
    courier = Create(6, TrainSlave);
    sensorCourier = Create(6, TrainSensorSlave);
    Send(sensorCourier, &t, sizeof(t), &t, sizeof(t));
    sensors = (bool*)t.args[1];
    sigkill = false;
    status2 = 0;
    msg.args = argument_buf;
    while (sigkill == false) {
        bytes = Receive(&callee, &t, sizeof(t));
        cmd = t.args[0];
        argument_buf[0] = cmd;
        /* switches on the command and validates it */
        status1 = 0;
        switch (cmd) {
            case TRAIN_GET_SENSOR:
                if (t.args[1] >= 'A' && t.args[1] <= 'A' + TRAIN_MODULE_COUNT - 1) {
                    if (t.args[2] >= 0 && t.args[2] <= TRAIN_SENSOR_COUNT) {
                        status1 = sensors[t.args[1] - 'A' + t.args[2]];
                    } else {
                        status1 = -2;
                    }
                } else {
                    status1 = -1;
                }
                break;
            case TRAIN_POLL_SENSORS:
                /* new sensor data */
                if (callee == sensorCourier) {
                    /* validate callee actually child */
                    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
                        if (sensors[i]) {
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A', i % 16);
                        }
                    }
                    status1 = status2;
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
                status1 = trainSpeed((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status1 == 0) {
                    debug("Setting Train %u at Speed %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or speed.\r\n");
                }
                break;
            case TRAIN_SWITCH:
                status1 = trainSwitch((unsigned int)t.args[1], (int)t.args[2]);
                Send(courier, &msg, sizeof(msg), &status2, sizeof(status2));
                printSwitch((unsigned int)t.args[1], (char)t.args[2]);
                if (status1 == 0) {
                    debug("Toggling Switch %u to State %c", t.args[1], toUpperCase(t.args[2]));
                } else {
                    printf("Error: Invalid state or switch.\r\n");
                }
                break;
            case TRAIN_AUX:
                status1 = trainAuxiliary((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status1 == 0) {
                    debug("Toggling auxiliary function %u for Train %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or auxiliary function.\r\n");
                }
                break;
            case TRAIN_RV:
                if (getTrain((unsigned int)t.args[1])) {
                    debug("Reversing train: %u", t.args[1]);
                    argument_buf[1] = t.args[1];
                    Send(courier, &msg, sizeof(msg), &status2, sizeof(status2));
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_LI:
                status1 = trainAuxiliary((unsigned int)t.args[1], TRAIN_LIGHT_OFFSET);
                if (status1 == 0) {
                    debug("Turning on/off the lights on train: %u", t.args[1]);
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_HORN:
                status1 = trainAuxiliary((unsigned int)t.args[1], TRAIN_HORN_OFFSET);
                if (status1 == 0) {
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
        Reply(callee, &status1, sizeof(status1));
    }

    Exit();
}
