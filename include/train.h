#ifndef __TRAIN_H__
#define __TRAIN_H__
#include <ts7200.h>
#include <types.h>
#include <track_node.h>

#define TRAIN                COM1
#define TRAIN_SWITCH_COUNT   22     /* switches on the board, upper four are special */
#define TRAIN_SENSOR_COUNT   16     /* number of sensors per module */
#define TRAIN_MODULE_COUNT   5      /* number of sensor modules */
#define TRAIN_MODULE_BASE    128    /* base addr for sensor modules */
#define TRAIN_SENSOR_BASE    192    /* sensor base addr */
#define MULTI_SWITCH_OFFSET  134    /* we have four multi switches */
#define TRAIN_LIGHT_OFFSET   16     /* offset for lights */
#define TRAIN_HORN_OFFSET    118    /* offset for horn */
#define TRAIN_HORN_OFF       112
#define TRAIN_AUX_REVERSE    15
#define TRAIN_MAX_SPEED      14

#define SWITCH_CHAR(x) ((x == DIR_STRAIGHT) ? 'S' : 'C')

struct __Train_t;


typedef struct {
    unsigned int state : 1;
    unsigned int id : 16;
} Switch_t;


typedef struct {
    char module;
    unsigned int id : 16;
} Sensor_t;


int trainSpeed(unsigned int tr, unsigned int speed);
int trainSwitch(unsigned int id, char state);
int trainReverse(unsigned int tr);
void turnOnTrainSet();
void turnOffTrainSet();
void setTrainSetState();
void initTrainSet();
int sensorToInt(char module, unsigned int id);
void pollSensors();
void resetSensors();
void turnOffSolenoid();
Switch_t *getSwitch(unsigned int id);
Sensor_t *getSensor(char module, unsigned int id);
Sensor_t *getSensorFromIndex(unsigned int index);

/* Deprecated */
void trbwputc(char ch);
int trbwflush();

#endif /* __TRAIN_H__ */
