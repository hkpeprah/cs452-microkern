#include <syscall.h>
#include <clock.h>
#include <controller.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>

#define WAIT_TIME(microPerTick, dist) ((dist * 1000 / microPerTick) + 10)

int TrSpeed(int tid, int speed) {
    TrainMessage_t msg = {.type = TRM_SPEED, .arg0 = speed};
    return Send(tid, &msg, sizeof(msg), NULL, 0);
}

int TrReverse(int tid) {
    TrainMessage_t msg = {.type = TRM_RV};
    return Send(tid, &msg, sizeof(msg), NULL, 0);
}

static inline unsigned int toMicroPerTick(unsigned int tr, unsigned int sp) {
    return sp;

    switch(tr) {
        case 45:

        case 47:

        case 48:

        case 49:

        case 50:

        default:
            error("BAD TRAIN NUMBER");
            return 0;
    }
}

static void TrainCourierTask() {
    int boss = MyParentTid();
    TrainMessage_t msg;
    msg.type = TRM_SENSOR_WAIT;

    for(;;) {
        Send(boss, &msg, sizeof(msg), &msg, sizeof(msg));
        switch (msg.type) {
            case TRM_SENSOR_WAIT:
                // wait on sensor with delay
                break;

            case TRM_TIME_WAIT:
                Delay(msg.arg1);
                break;

            default:
                error("TrainCourierTask: Invalid message type %d", msg.type);
        }
    }
}

void TrainTask() {
    int sensorCourier;
    bool waiting = 0;
    int result, sender;
    char cmdbuf[2];
    TrainMessage_t msg;

    unsigned int trainNumber, speed, aux;
    track_edge *currentEdge;
    track_node *dest;
    unsigned int edgeDistanceMM, currentTick, lastDistUpdateTick, microPerTick;
    Switch_t *sw;

    edgeDistanceMM = currentTick = lastDistUpdateTick = microPerTick = 0;

    result = Receive(&sender, &msg, sizeof(msg));

    if (result < 0 || msg.type != TRM_INIT) {
        error("TrainTask: Receive error: %d or incorrect msg type (not TRM_INIT): %d", result, msg.type);
    }

    trainNumber = msg.arg0;
    currentEdge = (track_edge*) msg.arg1;
    dest = currentEdge->dest;
    Reply(sender, NULL, 0);


    // TODO: are these the right priorities?
    sensorCourier = Create(5, TrainCourierTask);

    // TODO: initialization, first command, stuff

    for(;;) {
        result = Receive(&sender, &msg, sizeof(msg));

        if (result < 0) {
            error("TrainTask: Receive error: %d", result);
        }

        switch (msg.type) {
            case TRM_SENSOR_WAIT:
            case TRM_TIME_WAIT:
                if (waiting) {
                    // sensor tripped, do stuff
                    if (dest->num != msg.arg0) {
                        error("Expected dest node %d but received %d", dest->num, msg->arg0);
                    }

                    switch (dest->type) {
                        case NODE_SENSOR:
                        case NODE_MERGE:
                        case NODE_ENTER:
                        case NODE_EXIT:
                            currentEdge = &(dest->edge[DIR_AHEAD]);
                            break;
                        case NODE_BRANCH:
                            sw = getSwitch(dest->num);
                            currentEdge = &(dest->edge[sw->state]);
                            break;
                        case NODE_NONE:
                        default:
                            error("BAD NODE TYPE");
                    }

                    dest = currentEdge->dest;
                    edgeDistanceMM = 0;
                    lastDistUpdateTick = Time();

                    if (dest->type == NODE_SENSOR) {
                        msg.type = TRM_SENSOR_WAIT;
                        msg.arg0 = dest->num;
                    } else {
                        msg.type = TRM_TIME_WAIT;
                    }
                    msg.arg1 = WAIT_TIME(microPerTick, currentEdge->dist);
                    Reply(sensorCourier, &msg, sizeof(msg));
                } else {
                    // ignore
                }
                break;

            case TRM_SPEED:
                // update location
                // TODO: account for acceleration

                currentTick = Time();
                edgeDistanceMM += (currentTick - lastDistUpdateTick) * microPerTick / 1000;
                lastDistUpdateTick = currentTick;

                // send bytes
                speed = msg.arg0;
                cmdbuf[0] = speed;
                cmdbuf[1] = trainNumber;
                trnputs(cmdbuf, 2);
                microPerTick = toMicroPerTick(trainNumber, speed);
                break;

            case TRM_RV:
                break;

            case TRM_GET_LOCATION:
                currentTick = Time();
                edgeDistanceMM += (currentTick - lastDistUpdateTick) * microPerTick / 1000;
                lastDistUpdateTick = currentTick;
                msg.arg0 = (int) currentEdge;
                msg.arg1 = edgeDistanceMM;
                Reply(sender, &msg, sizeof(msg));

                break;

            default:
                error("TrainTask %d, incorrect msg type %d", trainNumber, msg.type);
        }
    }
}
