#include <sensor_server.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>
#include <util.h>
#include <random.h>

DECLARE_CIRCULAR_BUFFER(int);

static unsigned int sensor_server_tid = -1;

typedef enum {
    SENSOR_WAIT = 0,
    SENSOR_WAIT_TIMEOUT,
    SENSOR_WAIT_ANY,
    SENSOR_RETURNED,
    SENSOR_FREE,
    SENSOR_LAST_POLL
} SensorRequestType;


typedef struct {
    SensorRequestType type;
    uint32_t sensor;
    uint32_t timeout;
} SensorRequest_t;


typedef struct SensorQueue {
    int tid : 16;
    uint32_t timeout : 16;
} SensorQueue_t;


static void SensorSlave() {
    SensorRequest_t t;
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
    SensorRequest_t req, reply;
    int tid, callee, status, calleeByte;
    unsigned int i, timeout;
    unsigned int bytes, notifier, *sensors;
    unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    SensorQueue_t sensorQueue[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {{0}};
    CircularBuffer_int waitAnyQueue;
    volatile uint32_t lastPoll[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    (void)reply; /* TODO: May use this in future */

    initcb_int(&waitAnyQueue);
    RegisterAs(SENSOR_SERVER);
    sensor_server_tid = MyTid();
    notifier = Create(13, SensorSlave);

    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
        sensorQueue[i].tid = -1;
        sensorQueue[i].timeout = 0;
    }

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        if (bytes < 0) {
            error("SensorServer: Failed to receive message from Task %d with status %d", callee, bytes);
        }

        timeout = 0;
        switch (req.type) {
            case SENSOR_LAST_POLL:
                for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
                    *((uint32_t*)req.sensor + i) = lastPoll[i];
                }
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case SENSOR_WAIT_ANY:
                calleeByte = (int)callee;
                write_int(&waitAnyQueue, &calleeByte, 1);
                break;
            case SENSOR_WAIT_TIMEOUT:
                timeout = req.timeout;
            case SENSOR_WAIT:
                if (req.sensor < maxId && sensorQueue[req.sensor].tid < 0) {
                    status = 0;
                    sensorQueue[req.sensor].tid = callee;
                    sensorQueue[req.sensor].timeout = timeout;
                } else {
                    status = OTHER_TASK_WAITING_ON_SENSOR;
                    Reply(callee, &status, sizeof(status));
                }
                break;
            case SENSOR_FREE:
                Reply(callee, &status, sizeof(status));
                if (req.sensor > maxId || sensorQueue[req.sensor].tid < 0) {
                    status = INVALID_SENSOR;
                    break;
                }
                status = TIMER_TRIP;
                Reply(sensorQueue[req.sensor].tid, &status, sizeof(status));
                sensorQueue[req.sensor].tid = -1;
                break;
            case SENSOR_RETURNED:
                if (callee == notifier) {
                    int tripped_sensor = -1;
                    sensors = (uint32_t*)req.sensor;

                    for (i = 0; i < maxId; ++i) {
                        bool tripped = false;
                        tid = -1;
                        if (sensors[i] && !lastPoll[i]) {
                            Log("TRUE TRIP - %c%d", (i / TRAIN_SENSOR_COUNT) + 'A', (i % TRAIN_SENSOR_COUNT) + 1);
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT) + 1);      // index in module
                            tid = sensorQueue[i].tid;
                            sensorQueue[i].tid = -1;
                            status = SENSOR_TRIP;
                            tripped = true;
                        }

                        if (tid >= 0) {
                            Reply(tid, &status, sizeof(status));
                        } else if (tripped_sensor == -1 && tripped) {
                            /* assign the sensor to the one not being waited on */
                            tripped_sensor = i;
                        }
                        lastPoll[i] = sensors[i];
                    }

                    if ((status = tripped_sensor) >= 0) {
                        while (length_int(&waitAnyQueue) > 0) {
                            read_int(&waitAnyQueue, &calleeByte, 1);
                            Reply(calleeByte, &status, sizeof(status));
                        }
                    }
                    status = 1;
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
    SensorRequest_t msg;

    ASSERT(sensor_server_tid >= 0, "server TID not available");

    msg.type = SENSOR_WAIT_TIMEOUT;
    msg.sensor = id;
    msg.timeout = to;

    errno = Send(sensor_server_tid, &msg, sizeof(msg), &status, sizeof(status));

    ASSERT(errno >= 0, "Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);

    return status;
}

int WaitOnSensorN(unsigned int id) {
    int errno, status;
    SensorRequest_t wait;

    ASSERT(sensor_server_tid >= 0, "server TID not available");

    wait.type = SENSOR_WAIT;
    wait.sensor = id;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));

    ASSERT(errno >= 0, "Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);

    return status;
}


int WaitOnSensor(char module, unsigned int id) {
    return WaitOnSensorN(sensorToInt(module, id));
}


int WaitAnySensor() {
    int errno, status;
    SensorRequest_t wait;

    ASSERT(sensor_server_tid >= 0, "server TID not available");

    wait.type = SENSOR_WAIT_ANY;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));
    ASSERT(errno >= 0, "Error in send: %d got %d, sending to %d", MyTid(), errno, sensor_server_tid);

    return status;
}


int FreeSensor(unsigned int id) {
    int errno, status;
    SensorRequest_t wait;

    ASSERT(sensor_server_tid >= 0, "server TID not available");

    wait.type = SENSOR_FREE;
    wait.sensor = id;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));

    ASSERT(errno >= 0, "Error in send: %d got %d, sending to %d", MyTid(), errno, sensor_server_tid);

    return status;
}


int LastSensorPoll(unsigned int *data) {
    int errno, status;
    SensorRequest_t poll;

    ASSERT(sensor_server_tid >= 0, "server TID not available");

    poll.type = SENSOR_LAST_POLL;
    poll.sensor = (uint32_t)data;
    errno = Send(sensor_server_tid, &poll, sizeof(poll), &status, sizeof(status));

    ASSERT(errno >= 0, "Error in send: %d got %d, sending to %d", MyTid(), errno, sensor_server_tid);

    return status;
}
