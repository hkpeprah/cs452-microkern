#ifndef __TRAIN_H__
#define __TRAIN_H__
#include <ts7200.h>
#include <types.h>

#define TRAIN                COM1
#define TRAIN_SWITCH_COUNT   22     /* switches on the board, upper four are special */
#define TRAIN_SENSOR_COUNT   16     /* number of sensors per module */
#define TRAIN_MODULE_COUNT   5      /* number of sensor modules */
#define TRAIN_MODULE_BASE    128    /* base addr for sensor modules */
#define TRAIN_SENSOR_BASE    192    /* sensor base addr */
#define MULTI_SWITCH_OFFSET  134    /* we have four multi switches */
#define TRAIN_LIGHT_OFFSET   16     /* offset for lights */
#define TRAIN_HORN_OFFSET    17     /* offset for horn */


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
    TRAIN_GET_SENSOR,
    TRAIN_POLL_SENSORS,
    NUM_TRAIN_COMMANDS,
} TrainCommands;


typedef struct __Train_t {
    unsigned int id : 16;
    unsigned int speed : 8;
    unsigned int aux : 8;
    unsigned int lastSensor : 16;
    struct __Train_t *next;
    /* TODO: Direction or nextSensor ? */
} Train_t;


typedef struct {
    unsigned int id : 16;
    char state;
} Switch_t;


typedef struct {
    unsigned int module : 4;
    unsigned int id : 16;
} Sensor_t;


int trainSpeed(unsigned int, unsigned int);
int trainAuxiliary(unsigned int, unsigned int);
int trainReverse(unsigned int);
int trainSwitch(unsigned int, int);
void turnOnTrainSet();
void turnOffTrainSet();
void clearTrainSet();
void pollSensors();
void resetSensors();
void turnOffSolenoid();
Train_t *addTrain(unsigned int);
Train_t *getTrain(unsigned int);
Switch_t *getSwitch(unsigned int);
Sensor_t *getSensor(char, unsigned int);

/* Deprecated */
void trbwputc(char);
void trbwputs(char*);
void trnbwputs(char*, unsigned int);
char trbwflush();

#endif /* __TRAIN_H__ */
