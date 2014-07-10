#include <sensor_server.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>
#include <util.h>
#include <random.h>

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
    char calleeByte;
    int tid, callee, status;
    unsigned int i, timeout, sensor, time = 0;
    unsigned int bytes, notifier, *sensors;
    unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    SensorQueue_t sensorQueue[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {{0}};
    CircularBuffer_t waitAnyQueue;
    volatile uint32_t lastPoll[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    (void)reply; /* TODO: May use this in future */

    initcb(&waitAnyQueue);
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
        sensor = 0;
        switch (req.type) {
            case SENSOR_LAST_POLL:
                for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
                    *((uint32_t*)req.sensor + i) = lastPoll[i];
                }
                status = 1;
                Reply(callee, &status, sizeof(status));
                break;
            case SENSOR_WAIT_ANY:
                calleeByte = (char)callee;
                write(&waitAnyQueue, &calleeByte, 1);
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
                /* TODO: Would be optimal if knew which train triggered ? */
                if (callee == notifier) {
                    status = 1;
                    sensors = (uint32_t*)req.sensor;

                    // TODO: is this actually better than using Time all the time?
                    if (random_range(0, 9) == 0) {
                        // re-sync clock with probability 10%
                        time = Time();
                    } else {
                        // otherwise, roughly estimate the time. the sensor poll loop is roughly every 6 ticks
                        // a loss of granularity here, but it only applies for cases of error sensors and our
                        // time-based location re-sync in the train should handle it just fine
                        time += 6;
                    }

                    for (i = 0; i < maxId; ++i) {
                        tid = -1;
                        if (sensors[i] && !lastPoll[i]) {
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT) + 1);      // index in module
                            tid = sensorQueue[i].tid;
                            sensorQueue[i].tid = -1;
                            status = SENSOR_TRIP;
                            sensor = (sensor ? sensor : i);
                        } else if (sensorQueue[i].timeout > 0 && sensorQueue[i].timeout < time) {
                            tid = sensorQueue[i].tid;
                            sensorQueue[i].tid = -1;
                            status = TIMER_TRIP;
                        }

                        if (tid >= 0) {
                            Reply(tid, &status, sizeof(status));
                        }
                        lastPoll[i] = sensors[i];
                    }

                    if ((status = sensor)) {
                        while (length(&waitAnyQueue) > 0) {
                            read(&waitAnyQueue, &calleeByte, 1);
                            Reply(calleeByte, &status, sizeof(status));
                        }
                    }
                    sensor = 0;
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
    SensorRequest_t wait;

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
    SensorRequest_t wait;

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


int FreeSensor(unsigned int id) {
    int errno, status;
    SensorRequest_t wait;

    if (sensor_server_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_FREE;
    wait.sensor = id;
    errno = Send(sensor_server_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("FreeSensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);
        return -2;
    }

    return status;
}


int LastSensorPoll(unsigned int *data) {
    int errno, status;
    SensorRequest_t poll;

    if (sensor_server_tid < 0) {
        return -1;
    }

    poll.type = SENSOR_LAST_POLL;
    poll.sensor = (uint32_t)data;
    errno = Send(sensor_server_tid, &poll, sizeof(poll), &status, sizeof(status));

    if (errno < 0) {
        error("LastSensorPoll: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, sensor_server_tid);
        return -2;
    }

    return status;
}
