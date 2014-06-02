#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>
#include <server.h>
#include <clock.h>


typedef struct {
    unsigned int t;
    unsigned int n;
    unsigned int complete;
} DelayMessage;


void testTask();
void firstTask();

#endif /* __UTASKS__ */
