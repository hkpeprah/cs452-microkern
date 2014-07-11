#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__
#include <train.h>


typedef struct {
    unsigned int tr;
    unsigned int sp;
    unsigned int dist;
    unsigned int eta;
    unsigned int ata;
    const char *landmark;
    const char *nextmark;
} CalibrationSnapshot_t;


void initCalibration();
void printTrainSnapshot(CalibrationSnapshot_t *snapshot);

#endif /* __CALIBRATION_H__ */
