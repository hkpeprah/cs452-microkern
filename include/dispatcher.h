#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__
#include <types.h>
#include <track_data.h>
#include <track_node.h>

#define TRAIN_DISPATCHER         "TrainDispatcher"
#define NUM_OF_TRAINS            8
#define TIMEOUT_BUFFER           20

#define INVALID_TRAIN_ID         -1
#define INVALID_SPEED            -2
#define INVALID_AUXILIARY        -3
#define INVALID_SWITCH_ID        -4
#define INVALID_SWITCH_STATE     -5
#define INVALID_SENSOR_ID        -6
#define TRAIN_HAS_NO_CONDUCTOR   -7
#define OUT_OF_DISPATCHER_NODES  -8
#define INVALID_DESTINATION      -9
#define NO_PATH_EXISTS           -10
#define INVALID_REQUEST          -11
#define TRAIN_BUSY               -12
#define INVALID_AUX              -13


void Dispatcher();
int DispatchRoute(unsigned int tr, unsigned int sensor, unsigned int dist);
int DispatchAddTrain(unsigned int tr);
int DispatchAddTrainAt(unsigned int tr, char module, unsigned int id);
int DispatchStopRoute(unsigned int tr);
int DispatchTrainMove(unsigned int tr, unsigned int dist);
int DispatchGetTrainTid(unsigned int tr);
track_node *DispatchGetTrackNode(unsigned int id);
int DispatchReserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist);
int DispatchReserveTrack(unsigned int tr, track_node **track, unsigned int n);
int DispatchReleaseTrack(unsigned int tr, track_node **track, unsigned int n);

#endif /* __DISPATCHER_H__ */
