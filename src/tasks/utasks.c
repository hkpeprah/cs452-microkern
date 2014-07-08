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
#include <train_task.h>
#include <track_node.h>
#include <sensor_server.h>
#include <controller.h>


void firstTask() {
    int id;
    id = Create(15, NameServer);
    id = Create(15, ClockServer);
    id = Create(12, InputServer);
    id = Create(12, OutputServer);
    id = Create(13, TimerTask);
    id = Create(12, SensorServer);
    id = Create(10, TrainController);
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


void TrainUserTask() {
    bool sigkill;
    TrainMessage t;
    int status, cmd, callee, bytes, tid;

    sigkill = false;
    RegisterAs("TrainHandler");

    while (sigkill == false) {
        bytes = Receive(&callee, &t, sizeof(t));
        cmd = t.args[0];

        /* switches on the command and validates it */
        status = 0;
        switch (cmd) {
            case TRAIN_NULL:
                debug("Received NULL message from %u", callee);
                break;
            case TRAIN_WAIT:
                if (WaitOnSensor(t.args[1], t.args[2]) > 0) {
                    printf("Sensor Triggered: %c%u\r\n", t.args[1], t.args[2]);
                } else {
                    printf("Error: Invalid sensor passed.\r\n");
                }
                break;
            case TRAIN_GO:
                turnOnTrainSet();
                debug("Starting Train Controller");
                break;
            case TRAIN_STOP:
                debug("Stopping Train Controller");
                turnOffTrainSet();
                sigkill = true;
                break;
            case TRAIN_SPEED:
                tid = LookupTrain(t.args[1]);
                if (tid >= 0) {
                    status = TrSpeed(tid, t.args[2]);
                    if (status == -2) {
                        printf("Error: Train in route.  Cannot control.\r\n");
                    } else if (status < 0) {
                        printf("Error: Invalid train speed.\r\n");
                    } else {
                        debug("Setting speed %u for Train %u", t.args[1], t.args[2]);
                    }
                } else {
                    printf("Error: Invalid train.\r\n");
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
            case TRAIN_LI:
                t.args[2] = TRAIN_LIGHT_OFFSET;
                goto auxiliary;
                break;
            case TRAIN_HORN:
                t.args[2] = TRAIN_HORN_OFFSET;
                goto auxiliary;
                break;
        auxiliary:
            case TRAIN_AUX:
                tid = LookupTrain(t.args[1]);
                if (tid >= 0) {
                    if (TrAuxiliary(tid, t.args[2]) < 0) {
                        printf("Error: Invalid auxiliary function.\r\n");
                    } else {
                        debug("Toggling auxiliary function %u for Train %u", t.args[1], t.args[2]);
                    }
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_RV:
                tid = LookupTrain(t.args[1]);
                if (tid >= 0) {
                    status = TrReverse(tid);
                    if (status == -2) {
                        printf("Error: Train in route.  Cannot control.\r\n");
                    } else {
                        debug("Reversing train: %u", t.args[1]);
                    }
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            case TRAIN_ADD:
                status = AddTrainToTrack(t.args[1], t.args[2], t.args[3]);
                if (status < 0) {
                    printf("Error: Invalid train or sensor.\r\n");
                } else {
                    debug("Added Train %u to track near %c%u", t.args[1], t.args[2], t.args[3]);
                }
                break;
            case TRAIN_GOTO:
                t.args[4] = 0;
            case TRAIN_GOTO_AFTER:
                tid = LookupTrain(t.args[1]);
                if (tid >= 0) {
                    status = MoveTrainToDestination(tid, t.args[2], t.args[3], t.args[4]);
                    if (status < 0) {
                        error("TrainHandler: Error: Received %d sending.", status);
                        printf("Error: No path found to destination.\r\n");
                    } else {
                        debug("Found path for Train %u to destination %c%u", t.args[1], t.args[2], t.args[3]);
                    }
                } else {
                    printf("Error: Invalid train.\r\n");
                }
                break;
            default:
                error("TrainController: Received %d from %u", cmd, callee);
                status = -1;
        }
        Reply(callee, &status, sizeof(status));
    }

    Exit();
}
