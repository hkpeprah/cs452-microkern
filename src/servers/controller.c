#include <controller.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>
#include <train_task.h>

static unsigned int train_controller_tid = -1;

typedef enum {
    SENSOR_WAIT = 0,
    SENSOR_WAIT_ANY,
    SENSOR_RETURNED,
    NEAREST_EDGE,
    NUM_TRAIN_REQUESTS
} TrainRequest_t;

typedef struct __SensorQueue_t {
    uint32_t tid : 8;
} SensorQueue;


static void TrainSensorSlave() {
    TRequest_t t;
    int status, byte;
    unsigned int i, j, index, parent;
    volatile uint32_t sensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    parent = MyParentTid();

    debug("TrainSensorSlave: Tid %d", MyTid());
    t.type = SENSOR_RETURNED;
    t.sensor = (uint32_t)sensors;

    while (true) {
        pollSensors();
        for (i = 0; i < TRAIN_MODULE_COUNT; ++i) {
            index = i * TRAIN_SENSOR_COUNT;

            byte = trgetchar();
            for (j = 0; j < TRAIN_SENSOR_COUNT / 2; ++j) {
                sensors[index++] = EXTRACT_BIT(byte, (TRAIN_SENSOR_COUNT / 2) - j - 1) & 1;
            }

            byte = trgetchar();
            for (; j < TRAIN_SENSOR_COUNT; ++j) {
                sensors[index++] = EXTRACT_BIT(byte, TRAIN_SENSOR_COUNT - j - 1) & 1;
            }
        }
        Send(parent, &t, sizeof(t), &status, sizeof(status));
    }

    Exit();
}


void TrainController() {
    TRequest_t req;
    unsigned int i;
    int callee, status;
    unsigned int bytes, notifier, *sensors;
    track_node track[TRACK_MAX];
    const unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    SensorQueue waitChannel[maxId];
    volatile uint32_t lastPoll[maxId];

    init_track(track);
    memset(&waitChannel, 0, maxId * sizeof(SensorQueue));
    memset(&lastPoll, 0, maxId * sizeof(uint32_t));
    RegisterAs(TRAIN_CONTROLLER);
    train_controller_tid = MyTid();
    notifier = Create(13, TrainSensorSlave);

    for (i = 0; i < maxId; ++i) {
        waitChannel[i].tid = 0;
    }

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        if (bytes < 0) {
            error("TrainController: Failed to receive message from Task %d with status %d", callee, bytes);
        }
        switch (req.type) {
            case SENSOR_WAIT_ANY:
                req.sensor = maxId;
            case SENSOR_WAIT:
                if (req.sensor <= maxId && waitChannel[req.sensor].tid == 0) {
                    waitChannel[req.sensor].tid = callee;
                } else {
                    status = 0;
                    Reply(callee, &status, sizeof(status));
                }
                break;
            case NEAREST_EDGE:
                if (req.sensor < maxId) {
                    status = (int)(&track[req.sensor].edge[DIR_AHEAD]);
                } else {
                    status = 0;
                }
                Reply(callee, &status, sizeof(status));
                break;
            case SENSOR_RETURNED:
                /* TODO: Would be optimal if knew which train triggered ? */
                status = -1;
                if (callee == notifier) {
                    status = 1;
                    sensors = (uint32_t*)req.sensor;
                    for (i = 0; i < maxId; ++i) {
                        if (sensors[i] && !lastPoll[i]) {
                            status = i;
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A', (i % TRAIN_SENSOR_COUNT) + 1);
                            if (waitChannel[i].tid != 0) {
                                Reply(waitChannel[i].tid, &status, sizeof(status));
                                waitChannel[i].tid = 0;
                            }

                            // TODO: Should we look for sensors not assigned to trains?
                            if (waitChannel[maxId].tid != 0) {
                                Reply(waitChannel[maxId].tid, &status, sizeof(status));
                                waitChannel[maxId].tid = 0;
                            }
                        }
                        lastPoll[i] = sensors[i];
                    }
                }
                Reply(callee, &status, sizeof(status));
                break;
        }
    }

    Exit();
}


int WaitOnSensorN(unsigned int id) {
    int errno, status;
    TRequest_t wait;

    if (train_controller_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_WAIT;
    wait.sensor = id;
    errno = Send(train_controller_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("WaitOnSensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, train_controller_tid);
        return -2;
    }

    return status;
}


int WaitOnSensor(char module, unsigned int id) {
    return WaitOnSensorN(sensorToInt(module, id));
}


int WaitAnySensor() {
    int errno, status;
    TRequest_t wait;

    if (train_controller_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_WAIT_ANY;
    errno = Send(train_controller_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("WaitAnySensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, train_controller_tid);
        return -2;
    }

    return status;
}


track_edge *NearestSensorEdge(char module, unsigned int id) {
    int errno, edge;
    TRequest_t msg;

    if (train_controller_tid < 0) {
        return NULL;
    }

    msg.type = NEAREST_EDGE;
    msg.sensor = sensorToInt(module, id);
    errno = Send(train_controller_tid, &msg, sizeof(msg), &edge, sizeof(edge));

    if (errno < 0) {
        error("NearestSensorEdge: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, train_controller_tid);
        return NULL;
    }

    if (edge > 0) {
        return (track_edge*)edge;
    }
    return NULL;
}
