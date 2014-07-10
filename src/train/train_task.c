#include <stdlib.h>
#include <server.h>
#include <syscall.h>
#include <clock.h>
#include <sensor_server.h>
#include <track_node.h>
#include <train_task.h>
#include <term.h>
#include <train.h>
#include <train_speed.h>
#include <calibration.h>
#include <types.h>
#include <path.h>
#include <random.h>
#include <dispatcher.h>

typedef struct {
    bool valid;
    unsigned int start_speed : 8;
    unsigned int dest_speed : 8;
    unsigned int time_issued;
} TransitionState_t;


typedef struct __Train_t {
    unsigned int id : 16;
    int speed : 8;
    unsigned int aux : 16;
    track_edge *currentEdge;
    track_node *nextSensor;
    unsigned int edgeDistance : 16;
    unsigned int lastUpdateTick;
    unsigned int microPerTick : 16;
    TransitionState_t *transition;
} Train_t;


static void TrainTask();


int TrCreate(int priority, int tr) {
    int result, trainTask;

    if (!isValidTrainId(tr)) {
        return -1;
    }

    trainTask = Create(priority, TrainTask);
    return trainTask;
}


static void CalibrationSnapshot(Train_t *train) {
    CalibrationSnapshot_t snapshot;
    snapshot.tr = train->id;
    snapshot.sp = train->speed;
    snapshot.dist = train->edgeDistance;
    snapshot.landmark = train->currentEdge->src->name;
    snapshot.nextmark = train->currentEdge->dest->name;
    printTrainSnapshot(&snapshot);
}


static void TrainTimer() {
    TrainMessage_t msg = {.type = TRM_GET_LOCATION};
    int parent = MyParentTid();

    while (true) {
        Delay(5);
        Send(parent, &msg, sizeof(msg), NULL, 0);
    }
    Exit();
}


static void TrainTask() {
    Train_t train;
    TrainMessage_t request;
    TransitionState_t state;
    int status, bytes;
    unsigned int dispatcher, callee;

    /* block on receive waiting for parent to send message */
    dispatcher = MyParentTid();
    Receive(&callee, &request, sizeof(request));
    if (callee != dispatcher || request.type != TRM_INIT) {
        error("Train: Error: Received message of type %d from %d, expected %d from %d",
              request.type, callee, TRM_INIT, dispatcher);
        Exit();
    }

    /* initialize state */
    state.valid = false;
    state.start_speed = 0;
    state.dest_speed = 0;
    state.time_issued = 0;

    /* initialize the train structure */
    train.id = request.tr;
    train.speed = 0;
    train.aux = 0;
    train.currentEdge = (track_edge*)request.arg0;
    train.edgeDistance = 0;
    train.microPerTick = 0;
    train.lastUpdateTick = 0;
    train.transition = &state;

    Reply(callee, NULL, 0);

    while (true) {
    }
    Exit();
}
