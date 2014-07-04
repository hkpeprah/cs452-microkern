#include <sensor_server.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>
#include <util.h>

static unsigned int sensor_server_tid = -1;

typedef enum {
    SENSOR_WAIT = 0,
    SENSOR_WAIT_TIMEOUT,
    SENSOR_WAIT_ANY,
    SENSOR_RETURNED,
    NUM_TRAIN_REQUESTS
} SensorRequest_t;


typedef struct {
    SensorRequest_t type;
    uint32_t sensor;
    uint32_t timeout;
} TRequest_t;


typedef struct SensorQueue {
    int tid : 16;
    uint32_t timeout : 16;
} SensorQueue_t;

static void SensorSlave() {
    TRequest_t t;
    int status, byte;
    unsigned int i, j, index, parent;
    volatile uint32_t sensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    parent = MyParentTid();

    debug("SensorSlave: Tid %d", MyTid());
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


void SensorServer() {
    TRequest_t req, reply;
    bool hasNotify;
    unsigned int timeout;
    unsigned int time;
    unsigned int i;
    int callee, status;
    char calleeByte;
    unsigned int bytes, notifier, *sensors;
    int tid;
    unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    SensorQueue_t sensorQueue[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {{0}};
    CircularBuffer_t waitAnyQueue;
    volatile uint32_t lastPoll[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    (void)reply; /* TODO: May use this in future */

    initcb(&waitAnyQueue);

    RegisterAs(SENSOR_SERVER);
    sensor_server_tid = MyTid();
    notifier = Create(13, SensorSlave);

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        if (bytes < 0) {
            error("SensorServer: Failed to receive message from Task %d with status %d", callee, bytes);
        }

        timeout = 0;
        hasNotify = 0;

        switch (req.type) {
            case SENSOR_WAIT_ANY:
                calleeByte = (char) callee;
                write(&waitAnyQueue, &calleeByte, 1);
                break;
            case SENSOR_WAIT_TIMEOUT:
                timeout = req.timeout;
            case SENSOR_WAIT:
                if (req.sensor < maxId && sensorQueue[req.sensor].tid < 0) {
                    status = 0;
                    printf("%d waiting on %d with timeout %d\n", callee, req.sensor, timeout);
                    sensorQueue[req.sensor].tid = callee;
                    sensorQueue[req.sensor].timeout = timeout;
                } else {
                    status = OTHER_TASK_WAITING_ON_SENSOR;
                    Reply(callee, &status, sizeof(status));
                }
                break;
            case SENSOR_RETURNED:
                /* TODO: Would be optimal if knew which train triggered ? */
                if (callee == notifier) {
                    status = 1;
                    sensors = (uint32_t*)req.sensor;

                    time = Time();

                    for (i = 0; i < maxId; ++i) {
                        tid = -1;
                        if (sensors[i] && !lastPoll[i]) {
                            #if debug
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT) + 1);      // index in module
                            #endif

                            tid = sensorQueue[i].tid;
                            sensorQueue[i].tid = -1;

                            hasNotify = 1;
                            
                        } else if (sensorQueue[i].timeout > 0 && sensorQueue[i].timeout < time) {
                            tid = sensorQueue[i].tid;
                            sensorQueue[i].tid = -1;
                        }

                        if (tid >= 0) {
                            Reply(tid, &status, sizeof(status));
                        }
                        lastPoll[i] = sensors[i];
                    }

                    if (hasNotify) {
                        while (length(&waitAnyQueue) > 0) {
                            read(&waitAnyQueue, &calleeByte, 1);
                            Reply(calleeByte, &status, sizeof(status));
                        }
                    }
                } else {
                    status = -1;
                }
                Reply(callee, &status, sizeof(status));
                break;
        }
    }

    Exit();
}

int WaitWithTimeout(unsigned int id, unsigned int to) {
    int errno, status;
    TRequest_t msg;

    if (sensor_server_tid < 0) {
        return -1;
    }

    msg.type = SENSOR_WAIT_TIMEOUT;
    msg.sensor = id;
    msg.timeout = to;

    errno = Send(sensor_server_tid, &msg, sizeof(msg), &status, sizeof(status));

    if (errno < 0) {
        error("WaitWithTimeout: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);
        return -2;
    }

    return status;
}

int WaitOnSensorN(unsigned int id) {
    int errno, status;
    TRequest_t wait;

    if (sensor_server_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_WAIT;
    wait.sensor = id;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("WaitOnSensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);
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

    if (sensor_server_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_WAIT_ANY;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("WaitAnySensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);
        return -2;
    }

    return status;
}
