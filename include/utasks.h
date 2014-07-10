#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>
#include <server.h>
#include <clock.h>

#define USER_TRAIN_DISPATCH  "USER_TRAIN_HANDLER"

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
