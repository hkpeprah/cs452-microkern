#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__
#include <train.h>


typedef struct {
    unsigned int tr : 6;
    unsigned int sp : 14;
    unsigned int dist : 12;
    unsigned int eta;
    unsigned int ata;
    const char *landmark;
    const char *nextmark;
    const char *headResv;
    const char *tailResv;
} CalibrationSnapshot_t;


void initCalibration();
void printTrainSnapshot(CalibrationSnapshot_t *snapshot);
void clearTrainSnapshot();

#endif /* __CALIBRATION_H__ */
