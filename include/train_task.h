#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__

#include <track_node.h>

typedef enum {
    TRM_INIT,               // sent on creation, pass tr number (a0)  and init location (a1)
    TRM_EXIT,               // no args, stop the train
    TRM_SENSOR_WAIT,        // sensor in arg0, time in arg1
    TRM_TIME_WAIT,          // arg1 ticks to wait, arg0 not used
    TRM_FREE_COURIER,       // optimistic courier free from time/sensor wait
    TRM_SPEED,              // speed in arg0
    TRM_RV,                 // no args
    TRM_GET_LOCATION        // train_edge ptr in arg0, dist (mm) in arg1
} TrainMessageType;

typedef struct TrainMessage {
    TrainMessageType type;
    int arg0;
    int arg1;
} TrainMessage_t;

int TrCreate(int priority, int tr, track_edge *start);
int TrSpeed(int tid, int speed);
int TrReverse(int tid);
int TrGetLocation(int tid, TrainMessage_t *msg);

#endif
