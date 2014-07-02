#include <controller.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>

static unsigned int train_controller_tid = -1;


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
    TRequest_t req, reply;
    unsigned int i;
    int callee, status;
    TrainQueue bank[TRAIN_SENSOR_COUNT];
    TrainQueue *free, *tmp;
    unsigned int bytes, notifier, *sensors;
    unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    TrainQueue *sensorQueue[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};
    volatile uint32_t lastPoll[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    (void)reply; /* TODO: May use this in future */

    RegisterAs(TRAIN_CONTROLLER);
    train_controller_tid = MyTid();
    notifier = Create(13, TrainSensorSlave);

    free = NULL;
    for (i = 0; i < TRAIN_SENSOR_COUNT; ++i) {
        bank[i].next = free;
        free = &bank[i];
    }

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        if (bytes < 0) {
            error("TrainController: Failed to receive message from Task %d with status %d", callee, bytes);
        }

        switch (req.type) {
            case SENSOR_WAIT:
                if (free != NULL && req.sensor < maxId) {
                    status = 0;
                    tmp = free;
                    free = free->next;
                    tmp->tid = callee;
                    tmp->next = sensorQueue[req.sensor];
                    sensorQueue[req.sensor] =  tmp;
                } else {
                    status = 0;
                    Reply(callee, &status, sizeof(status));
                }
                break;
            case SENSOR_RETURNED:
                /* TODO: Would be optimal if knew which train triggered ? */
                if (callee == notifier) {
                    status = 1;
                    sensors = (uint32_t*)req.sensor;
                    for (i = 0; i < maxId; ++i) {
                        if (sensors[i] && !lastPoll[i]) {
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT) + 1);      // index in module
                            while (sensorQueue[i] != NULL) {
                                tmp = sensorQueue[i];
                                sensorQueue[i] = tmp->next;
                                tmp->next = free;
                                free = tmp;
                                Reply(tmp->tid, &status, sizeof(status));
                            }
                        }
                        lastPoll[i] = sensors[i];
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


int WaitOnSensor(char module, unsigned int id) {
    /*
     * interrupt a return value of 0 as the sensor not existing.
     */
    int errno, status;
    TRequest_t wait;

    if (train_controller_tid < 0) {
        return -1;
    }

    wait.type = SENSOR_WAIT;
    wait.sensor = sensorToInt(module, id);
    errno = Send(train_controller_tid, &wait, sizeof(wait), &status, sizeof(status));

    if (errno < 0) {
        error("WaitOnSensor: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, train_controller_tid);
        return -2;
    }

    return status;
}
