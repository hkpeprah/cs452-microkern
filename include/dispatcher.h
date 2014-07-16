#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__
#include <types.h>
#include <track_data.h>
#include <track_node.h>

#define TRAIN_DISPATCHER   "TrainDispatcher"
#define NUM_OF_TRAINS      8
#define TIMEOUT_BUFFER     20


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
    TRAIN_BUSY,
    NUM_DISPATCHER_ERROR_MESSAGES
} DispatcherErrorMessages_t;


void Dispatcher();
int DispatchTrainAuxiliary(unsigned int tr, unsigned int aux);
int DispatchTrainSpeed(unsigned int tr, unsigned int speed);
int DispatchRoute(unsigned int tr, unsigned int sensor, unsigned int dist);
int DispatchAddTrain(unsigned int tr);
int DispatchAddTrainAt(unsigned int tr, char module, unsigned int id);
int DispatchTrainReverse(unsigned int tr);
int DispatchStopRoute(unsigned int tr);
int DispatchTrainMove(unsigned int tr, unsigned int dist);
track_node *DispatchGetTrackNode(unsigned int id);
track_node *DispatchReserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist);
track_node *DispatchReserveTrack(unsigned int tr, track_node **track, unsigned int n);
track_node *DispatchReleaseTrack(unsigned int tr, track_node **track, unsigned int n);

#endif /* __DISPATCHER_H__ */
