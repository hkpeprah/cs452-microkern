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


void firstTask() {
    Create(15, NameServer);
    Create(15, ClockServer);
    Create(0, NullTask);
    Create(1, Shell);
    Create(5, trainUserTask);

    debug("FirstTask: Exiting");
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


void trainUserCourier() {
    unsigned int ut;
    TrainMessage msg;
    int status, callee, bytes;

    ut = MyParentTid();

    status = 1;
    while (true) {
        bytes = Receive(&callee, &msg, sizeof(msg));
        Reply(callee, &status, sizeof(status));
        if (ut == callee && bytes > 0) {
            switch (msg.args[0]) {
                case TRAIN_RV:
                    Delay(1);                    /* TODO: Calculate this */
                    trainReverse(msg.args[1]);
                    debug("Train reverse initiated.");
                    break;
                case TRAIN_SWITCH:
                    Delay(4);
                    turnOffSolenoid();
                    debug("Solenoid turned off.");
                    break;
            }
        }
    }

    Exit();
}


void trainUserTask() {
    TrainMessage t, courier;
    int cmd, callee, bytes;
    int status1, status2;
    int argument_buf[3];
    unsigned int revCourier, solCourier;

    RegisterAs("TrainHandler");
    revCourier = Create(6, trainUserCourier);
    solCourier = Create(6, trainUserCourier);

    turnOnTrain();
    clearTrainSet();

    status2 = 0;
    courier.args = argument_buf;
    while (true) {
        bytes = Receive(&callee, &t, sizeof(t));
        cmd = t.args[0];
        argument_buf[0] = cmd;

        /* switches on the command and validates it */
        status1 = 0;
        switch (cmd) {
            case TRAIN_GO:
                Reply(callee, &status1, sizeof(status1));
                turnOnTrain();
                debug("Starting Train Controller");
                break;
            case TRAIN_STOP:
                Reply(callee, &status1, sizeof(status1));
                turnOffTrain();
                debug("Stopping Train Controller");
                break;
            case TRAIN_SPEED:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainSpeed((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status1 == 0) {
                    debug("Setting Train %u at Speed %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or speed.\r\n");
                }
                break;
            case TRAIN_SWITCH:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainSwitch((unsigned int)t.args[1], (int)t.args[2]);
                Send(revCourier, &courier, sizeof(courier), &status2, sizeof(status2));
                if (status1 == 0) {
                    debug("Toggling Switch %u to State %c", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid state or switch.\r\n");
                }
                break;
            case TRAIN_AUX:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainAuxiliary((unsigned int)t.args[1], (unsigned int)t.args[2]);
                if (status1 == 0) {
                    debug("Toggling auxiliary function %u for Train %u", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid train or auxiliary function.\r\n");
                }
                break;
            case TRAIN_RV:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainSpeed((unsigned int)t.args[1], 0);
                if (status1 == 0) {
                    debug("Reversing train: %u", t.args[1]);
                    argument_buf[1] = t.args[1];
                    Send(revCourier, &courier, sizeof(courier), &status2, sizeof(status2));
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_LI:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainAuxiliary((unsigned int)t.args[1], TRAIN_AUX_LIGHTS);
                if (status1 == 0) {
                    debug("Turning on the lights on train: %u", t.args[1]);
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_HORN:
                Reply(callee, &status1, sizeof(status1));
                status1 = trainAuxiliary((unsigned int)t.args[1], TRAIN_AUX_HORN);
                if (status1 == 0) {
                    debug("Turning on/off horn on train: %u", t.args[1]);
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
        }
    }

    Exit();
}
