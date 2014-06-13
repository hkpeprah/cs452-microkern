#ifndef __TRAIN_H__
#define __TRAIN_H__
#include <ts7200.h>

#define TRAIN                COM1
#define TRAIN_AUX_LIGHTS     0
#define TRAIN_AUX_HORN       0
#define TRAIN_SWITCH_COUNT   22     /* switches on the board, upper four are special */
#define TRAIN_SENSOR_COUNT   16     /* number of sensors per module */
#define TRAIN_MODULE_COUNT   5      /* number of sensor modules */
#define TRAIN_MODULE_BASE    128    /* base addr for sensor modules */
#define TRAIN_SENSOR_BASE    192    /* sensor base addr */
#define MULTI_SWITCH_OFFSET  153    /* we have four multi switches */
#define TRAIN_LIGHT_OFFSET   16     /* offset for lights */
#define TRAIN_HORN_OFFSET    17     /* offset for horn */


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
    NUM_TRAIN_COMMANDS
} TrainCommands;


void clearTrainSet();
int trainSpeed(unsigned int, unsigned int);
int trainAuxiliary(unsigned int, unsigned int);
int trainReverse(unsigned int);
int trainSwitch(unsigned int, int);
void turnOnTrain();
void turnOffTrain();
void turnOffSolenoid();

/* Deprecated */
void trbwputc(char);
void trbwputs(char*);
char trbwflush();

#endif /* __TRAIN_H__ */
