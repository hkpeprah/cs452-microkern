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
    TrainMessage msg;
    int status, callee, bytes;
    Train_t *train;

    ut = MyParentTid();
    status = 1;

    bytes = Receive(&callee, &msg, sizeof(msg));
    if (bytes < 0) {
        error("TrainUserCourier: Got send %d from Task %d", bytes, callee);
    }

    Reply(callee, &status, sizeof(status));
    if (ut == callee && bytes > 0) {
        switch (msg.args[0]) {
            case TRAIN_RV:
                if ((train = getTrain(msg.args[1]))) {
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

    Exit();
}


void TrainUserTask() {
    TrainMessage t, msg;
    int cmd, callee, bytes;
    int status1, status2;
    int argument_buf[3];
    unsigned int i;
    unsigned int courier;

    RegisterAs("TrainHandler");
    turnOnTrainSet();
    clearTrainSet();

    status2 = 0;
    msg.args = argument_buf;
    while (true) {
        bytes = Receive(&callee, &t, sizeof(t));
        cmd = t.args[0];
        argument_buf[0] = cmd;

        /* switches on the command and validates it */
        status1 = 0;
        if (cmd == TRAIN_EVERYTHING) {
            for (i = 16; i < 32; ++i) {
                printf("Auxiliary function: %d\r\n", i);
                status1 = trainAuxiliary((unsigned int)t.args[1], i);
                Delay(100);
            }
        }

        Reply(callee, &status1, sizeof(status1));
        switch (cmd) {
            case TRAIN_GO:
                turnOnTrainSet();
                debug("Starting Train Controller");
                break;
            case TRAIN_STOP:
                turnOffTrainSet();
                debug("Stopping Train Controller");
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
                courier = Create(6, TrainSlave);
                Send(courier, &msg, sizeof(msg), &status2, sizeof(status2));
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
                    courier = Create(6, TrainSlave);
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
    }

    Exit();
}
