#ifndef __SENSOR_SERVER_H__
#define __SENSOR_SERVER_H__
#include <types.h>

#define SENSOR_SERVER "SENSOR_SERVER"
#define OTHER_TASK_WAITING_ON_SENSOR -1

void SensorServer();
int WaitWithTimeout(unsigned int id, unsigned int timeout);
int WaitOnSensorN(unsigned int);
int WaitOnSensor(char, unsigned int);
int WaitAnySensor();

#endif /* __SENSOR_SERVER_H__ */
