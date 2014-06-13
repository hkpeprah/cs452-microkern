#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>
#include <server.h>
#include <clock.h>


typedef struct {
    unsigned int t : 8;
    unsigned int n : 8;
    unsigned int complete : 8;
} DelayMessage;

typedef struct {
    int *args;
} TrainMessage;


void testTask();
void testTask2();
void firstTask();
void timerTask();
void TrainUserTask();
void TrainSlave();

#endif /* __UTASKS__ */
