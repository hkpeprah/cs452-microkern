#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>
#include <server.h>
#include <clock.h>

#define USER_TRAIN_DISPATCH  "USER_TRAIN_HANDLER"

typedef enum {
    TRAIN_CMD_GO = 13,
    TRAIN_CMD_STOP,
    TRAIN_CMD_SPEED,
    TRAIN_CMD_AUX,
    TRAIN_CMD_RV,
    TRAIN_CMD_LI,
    TRAIN_CMD_SWITCH,
    TRAIN_CMD_HORN,
    TRAIN_CMD_ADD,
    TRAIN_CMD_ADD_AT,
    TRAIN_CMD_GOTO,
    TRAIN_CMD_GOTO_AFTER,
    TRAIN_CMD_GOTO_STOP
} TrainCommandTypes;

typedef struct {
    unsigned int t : 8;
    unsigned int n : 8;
    unsigned int complete : 8;
} DelayMessage;

typedef struct ControllerMessage {
    int *args;
} ControllerMessage_t;


void firstTask();
void TimerTask();
void TrainUserTask();

#endif /* __UTASKS__ */
