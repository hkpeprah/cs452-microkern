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
#define TRAIN_HORN_OFFSET    17     /* offset for horn */
#define TRAIN_MAX_SPEED      14


struct __Train_t;

typedef enum {
    TRAIN_NULL = 0,
    TRAIN_GO,
    TRAIN_STOP,
    TRAIN_SPEED,
    TRAIN_SWITCH,
    TRAIN_AUX,
    TRAIN_RV,
    TRAIN_LI,
    TRAIN_HORN,
    TRAIN_ADD,
    TRAIN_WAIT,
    NUM_TRAIN_COMMANDS,
} TrainCommands;

#define SWITCH_CHAR(x) ((x == DIR_STRAIGHT) ? 'S' : 'C')

typedef struct {
    unsigned int state : 1;
    unsigned int id : 16;
} Switch_t;


typedef struct {
    char module;
    unsigned int id : 16;
} Sensor_t;


int trainSwitch(unsigned int, char);
void turnOnTrainSet();
void turnOffTrainSet();
void clearTrainSet();
int sensorToInt(char, unsigned int);
void pollSensors();
void resetSensors();
void turnOffSolenoid();
Switch_t *getSwitch(unsigned int);
Sensor_t *getSensor(char, unsigned int);
Sensor_t *getSensorFromIndex(unsigned int);

/* Deprecated */
void trbwputc(char);
int trbwflush();

#endif /* __TRAIN_H__ */
