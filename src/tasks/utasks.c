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
#include <path.h>

#define RESERVE   0
#define RELEASE   1


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


static int trackReservation(int type, unsigned int tr, int start, int end) {
    track_node *path[32] = {0};
    track_node *startNode = DispatchGetTrackNode(start);
    track_node *endNode = DispatchGetTrackNode(end);

    if (startNode == NULL || endNode == NULL) {
        return INVALID_SENSOR_ID;
    }

    debug("got reservation %d for train %d from %s to %s", type, tr, startNode->name, endNode->name);

    unsigned int pathDist;
    int pathLen = findPath(tr, &(startNode->edge[DIR_STRAIGHT]), endNode, path, 32, &pathDist);

    track_node *lastNode;

    switch(type) {
        case RESERVE:
            lastNode = DispatchReserveTrack(tr, path, pathLen);
            break;
        case RELEASE:
            lastNode = DispatchReleaseTrack(tr, path, pathLen);
        default:
            return -1337;
    }

    if (lastNode) {
        printf("reserved up to node %s\n", lastNode->name);
    } else {
        printf("no nodes reserved");
    }
    return 0;
}


void TrainUserTask() {
    ControllerMessage_t req;
    unsigned int dispatcher;
    int status, cmd, callee, bytes, sensor1, sensor2, trainTid;

    RegisterAs(USER_TRAIN_DISPATCH);
    dispatcher = WhoIs(TRAIN_DISPATCHER);
    notice("TrainUserTask: Tid %u, TrainUserTask's Dispatcher: %u", MyTid(), dispatcher);

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        status = 0;

        switch ((cmd = req.args[0])) {
            case TRAIN_CMD_GO:
                turnOnTrainSet();
                debug("Starting Train Controller");
                break;
            case TRAIN_CMD_STOP:
                debug("Stopping Train Controller");
                turnOffTrainSet();
                break;
            case TRAIN_CMD_LI:
                req.args[2] = TRAIN_LIGHT_OFFSET;
                goto auxiliary;
                break;
            case TRAIN_CMD_HORN:
                req.args[2] = TRAIN_HORN_OFFSET;
                goto auxiliary;
                break;
            case TRAIN_CMD_ADD:
                status = DispatchAddTrain(req.args[1]);
                break;
            case TRAIN_CMD_ADD_AT:
                status = DispatchAddTrainAt(req.args[1], req.args[2], req.args[3]);
                break;
            case TRAIN_CMD_SPEED:
                if ((trainTid = DispatchGetTrainTid(req.args[1])) >= 0) {
                    status = TrSpeed(trainTid, req.args[2]);
                } else {
                    status = trainTid;
                }
                break;
            case TRAIN_CMD_AUX:
        auxiliary:
                if ((trainTid = DispatchGetTrainTid(req.args[1])) >= 0) {
                    status = TrAuxiliary(trainTid, req.args[2]);
                } else {
                    status = trainTid;
                }
                break;
            case TRAIN_CMD_MOVE:
                status = DispatchTrainMove(req.args[1], req.args[2]);
                break;
            case TRAIN_CMD_RV:
                if ((trainTid = DispatchGetTrainTid(req.args[1])) >= 0) {
                    status = TrReverse(trainTid);
                } else {
                    status = trainTid;
                }
                break;
            case TRAIN_CMD_GOTO_STOP:
                status = DispatchStopRoute(req.args[1]);
                break;
            case TRAIN_CMD_SWITCH:
                status = trainSwitch((unsigned int)req.args[1], (int)req.args[2]);
                if (status == 0) {
                    Delay(30);
                    turnOffSolenoid();
                }
                break;
            case TRAIN_CMD_GOTO:
                req.args[4] = 0;
            case TRAIN_CMD_GOTO_AFTER:
                if ((sensor1 = sensorToInt(req.args[2], req.args[3])) >= 0) {
                    status = DispatchRoute(req.args[1], sensor1, req.args[4]);
                } else {
                    status = INVALID_SENSOR_ID;
                }
                break;
            case TRAIN_CMD_RESERVE:
                if ((sensor1 = sensorToInt(req.args[2], req.args[3])) >= 0 && (sensor2 = sensorToInt(req.args[2], req.args[3])) >= 0) {
                    status = trackReservation(RESERVE, req.args[1], sensor1, sensor2);
                } else {
                    status = INVALID_SENSOR_ID;
                }
                break;
            case TRAIN_CMD_RELEASE:
                if ((sensor1 = sensorToInt(req.args[2], req.args[3])) >= 0 && (sensor2 = sensorToInt(req.args[2], req.args[3])) >= 0) {
                    status = trackReservation(RELEASE, req.args[1], sensor1, sensor2);
                } else {
                    status = INVALID_SENSOR_ID;
                }
                break;
            default:
                error("TrainController: Error: Received %d from %u", cmd, callee);
                status = -1;
                Reply(callee, &status, sizeof(status));
                continue;
        }

        switch (status) {
            case 0: case 1:
                break;
            case INVALID_AUXILIARY:
                printf("Error: Invalid auxiliary number.\r\n");
                break;
            case INVALID_TRAIN_ID:
                printf("Error: Invalid train number.  Did you remember to do add?\r\n");
                break;
            case INVALID_SPEED:
                printf("Error: Invalid train speed.\r\n");
                break;
            case INVALID_SWITCH_ID:
                printf("Error: Switch does not exist.\r\n");
                break;
            case INVALID_SWITCH_STATE:
                printf("Error: Switch state is not one of (C)urved or (S)traight.\r\n");
                break;
            case INVALID_SENSOR_ID:
                printf("Error: Sensor does not exist.\r\n");
                break;
            case TRAIN_BUSY:
                printf("Error: Train is currently busy routing.\r\n");
                break;
            case TRAIN_HAS_NO_CONDUCTOR:
                printf("Error: Train does not have a conductor.\r\n");
                break;
            case OUT_OF_DISPATCHER_NODES:
                printf("Error: Out of slots for new trains.\r\n");
                break;
            case INVALID_DESTINATION:
                printf("Error: Invalid destination specified.\r\n");
                break;
            case NO_PATH_EXISTS:
                printf("Error: No path found to specified destination.\r\n");
                break;
            default:
                error("TrainController: Error: Unknown status received %d", status);
        }
        Reply(callee, &status, sizeof(status));
    }
    Exit();
}
