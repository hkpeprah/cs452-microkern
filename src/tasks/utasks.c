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
#include <dispatcher.h>


void firstTask() {
    int id;
    id = Create(15, NameServer);
    id = Create(15, ClockServer);
    id = Create(12, InputServer);
    id = Create(12, OutputServer);
    id = Create(13, TimerTask);
    id = Create(12, SensorServer);
    id = Create(10, Dispatcher);
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
    TrainMessage_t message;
    ControllerMessage_t req;
    int status, cmd, callee, bytes;
    unsigned int dispatcher;

    RegisterAs("TrainHandler");
    dispatcher = WhoIs(TRAIN_DISPATCHER);

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        status = 0;

        switch ((cmd = req.args[0])) {
            case TRM_GO:
                turnOnTrainSet();
                debug("Starting Train Controller");
                break;
            case TRM_STOP:
                debug("Stopping Train Controller");
                turnOffTrainSet();
                break;
            case TRM_LI:
                req.args[2] = TRAIN_LIGHT_OFFSET;
                break;
            case TRM_HORN:
                req.args[2] = TRAIN_HORN_OFFSET;
                break;
            case TRM_SPEED:
            case TRM_AUX:
            case TRM_RV:
                CreateDispatcherMessage(&message, cmd, req.args[1], req.args[2], req.args[3]);
                Send(dispatcher, &message, sizeof(message), &status, sizeof(status));
                break;
            case TRM_SWITCH:
                status = trainSwitch((unsigned int)req.args[1], (int)req.args[2]);
                if (status == 0) {
                    printSwitch((unsigned int)req.args[1], (char)req.args[2]);
                    Delay(30);
                    turnOffSolenoid();
                }
                break;
            case TRM_GOTO:
                req.args[4] = 0;
            case TRM_GOTO_AFTER:
                message.tr = req.args[1];
                message.arg0 = req.args[2];
                message.arg1 = req.args[3];
                message.arg2 = req.args[4];
                Send(dispatcher, &message, sizeof(message), &status, sizeof(status));
                break;
            case TRM_ADD:
                break;
            case TRM_ADD_AT:
                break;
            default:
                error("TrainController: Error: Received %d from %u", cmd, callee);
                status = -1;
                Reply(callee, &status, sizeof(status));
                continue;
        }

        switch (status) {
            case INVALID_TRAIN_ID:
                printf("Error: Invalid train number.  Did you remember to do add?");
                break;
            case INVALID_SPEED:
                printf("Error: Invalid train speed.");
                break;
            case INVALID_SWITCH_ID:
                printf("Error: Switch does not exist.");
                break;
            case INVALID_SWITCH_STATE:
                printf("Error: Switch state is not one of (C)urved or (S)traight.");
                break;
            case INVALID_SENSOR_ID:
                printf("Error: Sensor does not exist.");
                break;
            case TRAIN_BUSY:
                printf("Error: Train is currently busy routing.");
                break;
            case TRAIN_HAS_NO_CONDUCTOR:
                printf("Error: Train does not have a conductor.");
                break;
            case OUT_OF_DISPATCHER_NODES:
                printf("Error: Out of slots for new trains.");
                break;
            default:
                error("TrainController: Error: Unknown status received %d", status);
        }
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}
