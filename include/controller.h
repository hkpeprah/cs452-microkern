#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__
#include <types.h>

typedef enum {
    SENSOR_WAIT = 0,
    SENSOR_RETURNED,
    NUM_TRAIN_REQUESTS
} TrainRequest_t;


typedef struct {
    short type;
    uint32_t *sensor;
} TRequest_t;


typedef struct TrainQueue_t {
    uint32_t tid : 8;
    struct TrainQueue_t *next;
} TrainQueue;


void TrainController();

#endif /* __TRAIN_CONTROLLER_H__ */
