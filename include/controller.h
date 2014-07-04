#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__
#include <types.h>

#define TRAIN_CONTROLLER "TrainController"


typedef struct {
    short type;
    uint32_t sensor;
} TRequest_t;


void TrainController();
int WaitOnSensorN(unsigned int);
int WaitOnSensor(char, unsigned int);
int WaitAnySensor();

#endif /* __TRAIN_CONTROLLER_H__ */
