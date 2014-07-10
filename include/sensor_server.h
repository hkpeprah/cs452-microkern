#ifndef __SENSOR_SERVER_H__
#define __SENSOR_SERVER_H__
#include <types.h>

#define SENSOR_SERVER                   "SENSOR_SERVER"
#define OTHER_TASK_WAITING_ON_SENSOR    -1
#define INVALID_SENSOR                  -2
#define SENSOR_TRIP                     1
#define TIMER_TRIP                      2

void SensorServer();
int WaitWithTimeout(unsigned int id, unsigned int timeout);
int WaitOnSensorN(unsigned int id);
int WaitOnSensor(char module, unsigned int id);
int WaitAnySensor();
int FreeSensor(unsigned int id);
int LastSensorPoll(unsigned int *data);

#endif /* __SENSOR_SERVER_H__ */
