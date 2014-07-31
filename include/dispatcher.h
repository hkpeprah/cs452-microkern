#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__
#include <types.h>
#include <track_data.h>
#include <track_node.h>

#define TRAIN_DISPATCHER          "TrainDispatcher"
#define NUM_OF_TRAINS             8
#define TIMEOUT_BUFFER            20

#define INVALID_TRAIN_ID          -707
#define INVALID_SPEED             -708
#define INVALID_AUXILIARY         -709
#define INVALID_SWITCH_ID         -710
#define INVALID_SWITCH_STATE      -711
#define INVALID_SENSOR_ID         -712
#define TRAIN_HAS_NO_CONDUCTOR    -713
#define OUT_OF_DISPATCHER_NODES   -714
#define INVALID_DESTINATION       -715
#define NO_PATH_EXISTS            -716
#define INVALID_REQUEST           -717
#define TRAIN_BUSY                -718
#define INVALID_AUX               -719


void Dispatcher();
int DispatchRoute(uint32_t tr, uint32_t sensor, uint32_t dist);
int DispatchAddTrain(uint32_t tr);
int DispatchReAddTrain(uint32_t tr);
int DispatchAddTrainAt(uint32_t tr, char module, uint32_t id);
int DispatchStopRoute(uint32_t tr);
int DispatchTrainMove(uint32_t tr, uint32_t dist);
int DispatchGetTrainTid(uint32_t tr);
track_node *DispatchGetTrackNode(uint32_t id);
int DispatchTrackFree(uint32_t tr, int sensor);
int DispatchReserveTrackDist(uint32_t tr, track_node **track, uint32_t n, int *dist);
int DispatchReserveTrack(uint32_t tr, track_node **track, uint32_t n);
int DispatchReleaseTrack(uint32_t tr, track_node **track, uint32_t n);

#endif /* __DISPATCHER_H__ */
