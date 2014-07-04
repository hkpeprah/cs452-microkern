#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__
#include <types.h>
#include <track_data.h>
#include <track_node.h>

#define TRAIN_CONTROLLER "TrainController"

void TrainController();
track_edge *NearestSensorEdge(char, unsigned int);

#endif /* __TRAIN_CONTROLLER_H__ */
