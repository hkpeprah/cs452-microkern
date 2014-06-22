#include <controller.h>
#include <syscall.h>
#include <server.h>
#include <train.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>

static unsigned int train_controller_tid = -1;


static void TrainSensorSlave() {
    bool bit;
    TRequest_t t;
    int status, byte1, byte2;
    unsigned int i, j, parent;
    volatile uint32_t sensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};

    parent = MyParentTid();

    debug("TrainSensorSlave: Tid %d", MyTid());
    t.type = SENSOR_RETURNED;
    t.sensor = (uint32_t)sensors;

    while (true) {
        pollSensors();
        for (i = 0; i < TRAIN_MODULE_COUNT; ++i) {
            byte1 = trgetchar();
            byte2 = trgetchar();
            for (j = 0; j < TRAIN_SENSOR_COUNT; ++j) {
                if (j < TRAIN_SENSOR_COUNT / 2) {
                    /* first byte corresponds to sensors 1 - 8 */
                    bit = EXTRACT_BIT(byte1, (TRAIN_SENSOR_COUNT / 2) - j);
                } else {
                    /* lower byte corresponds to sensors 8 - 16 */
                    bit = EXTRACT_BIT(byte2, TRAIN_SENSOR_COUNT - j);
                }
                sensors[(i * TRAIN_SENSOR_COUNT) + j] = (bool)(bit & 1);
            }
        }
        Send(parent, &t, sizeof(t), &status, sizeof(status));
        Delay(1);
        resetSensors();
    }

    Exit();
}


void TrainController() {
    TRequest_t req, reply;
    unsigned int i;
    int callee, status;
    TrainQueue bank[TRAIN_SENSOR_COUNT];
    TrainQueue *free, *tmp;
    TrainQueue *sensorQueue[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT] = {0};
    unsigned int bytes, notifier, maxId, *sensors;

    (void)reply; /* TODO: May use this in future */

    RegisterAs("TrainController");
    train_controller_tid = MyTid();
    notifier = Create(13, TrainSensorSlave);

    free = NULL;
    maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
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
                        if (sensors[i]) {
                            printSensor((i / TRAIN_SENSOR_COUNT) + 'A',     // sensor module index
                                        (i % TRAIN_SENSOR_COUNT));          // index in module 
                            while (sensorQueue[i] != NULL) {
                                tmp = sensorQueue[i];
                                sensorQueue[i] = tmp->next;
                                Reply(tmp->tid, &status, sizeof(status));
                            }
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
