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
    CNTRL_GOTO = 0,
    CNTRL_STOP,
    CNTRL_ADD,
    CNTRL_NEAREST_EDGE,
    NUM_TRAIN_CNTRL_MESSAGES
} TrainControllerMessageType;

typedef struct {
    unsigned int tid;
    unsigned int tr_number;
} TrainCtrnl_t;

typedef struct {
    TrainControllerMessageType type;
    int arg0;
    int arg1;
    int arg2;
} TrainCntrlMessage_t;


void TrainController() {
    track_edge *edge;
    TrainCtrnl_t trains[7];
    int bytes, repl, callee, tid;
    unsigned int id, i;
    track_node track[TRACK_MAX];
    TrainCntrlMessage_t request, message;

    /* TODO: we will use this eventually... */
    (void)message;

    init_track(track);
    RegisterAs(TRAIN_CONTROLLER);
    train_controller_tid = MyTid();
    setTrainSetState();

    id = 0;
    while (true) {
        bytes = Receive(&callee, &request, sizeof(request));
        if (bytes < 0) {
            error("TrainController: Error: Failed to receive message from Task %d with status %d", callee, bytes);
        }

        repl = 0;
        switch (request.type) {
            case CNTRL_GOTO:
                /* tell the train controller to route a train to a location */
                break;
            case CNTRL_STOP:
                /* tell the train controller to stop routing the specified train */
                break;
            case CNTRL_NEAREST_EDGE:
                if (request.arg0 < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT) {
                    repl = (int)(&track[request.arg0].edge[DIR_AHEAD]);
                }
                break;
            case CNTRL_ADD:
                /* add a train to the track */
                i = sensorToInt(request.arg1, request.arg2);
                repl = -1;
                if (i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT) {
                    edge = &track[i].edge[DIR_AHEAD];
                    for (i = 0; i < id; ++i) {
                        if (trains[i].tr_number == request.arg0) {
                            break;
                        }
                    }
                    if (i == id && id < 7) {
                        tid = TrCreate(6, request.arg0, edge);
                        notice("TrainController: Notice: Received %d in create", tid);
                        if (tid > 0) {
                            trains[id].tid = tid;
                            trains[id].tr_number = request.arg1;
                            id++;
                            repl = 1;
                        }
                    }
                }
                break;
            default:
                repl = -1;
        }
        Reply(callee, &repl, sizeof(repl));
    }
    Exit();
}


track_edge *NearestSensorEdge(char module, unsigned int id) {
    int errno, edge;
    TrainCntrlMessage_t msg;

    if (train_controller_tid < 0) {
        return NULL;
    }

    msg.type = CNTRL_NEAREST_EDGE;
    msg.arg0 = sensorToInt(module, id);
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


int AddTrainToTrack(unsigned int tr, char module, unsigned int id) {
    int errno, status;
    TrainCntrlMessage_t msg;

    if (train_controller_tid < 0) {
        return -1;
    }

    msg.type = CNTRL_ADD;
    msg.arg0 = tr;
    msg.arg1 = module;
    msg.arg2 = id;
    errno = Send(train_controller_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("AddTrainToTrack: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, train_controller_tid);
        return -2;
    }

    return status;
}
