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
    NEAREST_EDGE,
    NUM_TRAIN_REQUESTS
} TrainRequest_t;

typedef struct {
    TrainRequest_t type;
    uint32_t sensor;
} TRequest_t;

void TrainController() {
    TRequest_t req, reply;
    int callee, status;
    unsigned int bytes;
    unsigned int maxId = TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT;
    track_node track[TRACK_MAX];

    (void)reply; /* TODO: May use this in future */

    init_track(track);
    RegisterAs(TRAIN_CONTROLLER);
    train_controller_tid = MyTid();

    while (true) {
        bytes = Receive(&callee, &req, sizeof(req));
        if (bytes < 0) {
            error("TrainController: Failed to receive message from Task %d with status %d", callee, bytes);
        }

        switch (req.type) {
            case NEAREST_EDGE:
                if (req.sensor < maxId) {
                    status = (int)(&track[req.sensor].edge[DIR_AHEAD]);
                } else {
                    status = 0;
                }
                Reply(callee, &status, sizeof(status));
                break;
            case NUM_TRAIN_REQUESTS:
            default:
                error("TrainController: bad request type");
        }
    }
    Exit();
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
