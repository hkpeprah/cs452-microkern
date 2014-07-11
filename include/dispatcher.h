#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__
#include <types.h>
#include <track_data.h>
#include <track_node.h>

#define TRAIN_DISPATCHER   "TrainDispatcher"
#define NUM_OF_TRAINS      8
#define TIMEOUT_BUFFER     20


typedef enum {
    TRM_INIT = 1337,
    TRM_EXIT,
    TRM_SENSOR_WAIT,
    TRM_TIME_WAIT,
    TRM_TIME_READY,
    TRM_SPEED,
    TRM_AUX,
    TRM_RV,
    TRM_GET_LOCATION,
    TRM_GET_SPEED,
    TRM_GET_PATH,
    TRM_DIR,
    TRM_GO,
    TRM_GOTO,
    TRM_GOTO_AFTER,
    TRM_STOP,
    TRM_ADD,
    TRM_ADD_AT,
    TRM_SWITCH,
    TRM_LI,
    TRM_HORN,
    TRM_BUSY,
    TRM_GOTO_STOP,
    TRM_RESERVE_TRACK,
    TRM_RELEASE_TRACK
} TrainMessageType;


typedef enum {
    INVALID_TRAIN_ID = 42,
    INVALID_SPEED,
    INVALID_AUXILIARY,
    INVALID_SWITCH_ID,
    INVALID_SWITCH_STATE,
    INVALID_SENSOR_ID,
    TRAIN_HAS_NO_CONDUCTOR,
    OUT_OF_DISPATCHER_NODES,
    INVALID_DESTINATION,
    NO_PATH_EXISTS,
    INVALID_REQUEST,
    TRAIN_BUSY
} DispatcherErrorMessages_t;


void Dispatcher();
int SendDispatcherMessage(TrainMessageType type, unsigned int tr, int arg0, int arg1);

#endif /* __DISPATCHER_H__ */
