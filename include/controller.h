#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__
#include <types.h>

#define TRAIN_CONTROLLER   "TrainController"
#define SENSOR_CONTROLLER  "SensorController"

typedef struct {
    short type;
    uint32_t sensor;
} TRequest_t;


void TrainController();
void SensorController();
int WaitOnSensorN(unsigned int num);
int WaitOnSensor(char module, unsigned int id);
int WaitAnySensor();
int AddTrainToTrack(unsigned int tr, char module, unsigned int id);
track_edge *NearestSensorEdge(char module, unsigned int id);

#endif /* __TRAIN_CONTROLLER_H__ */
