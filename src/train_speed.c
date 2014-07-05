#include <train_speed.h>
#include <types.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>

#define MEASUREMENT_TOTAL   15
#define TRAIN_COUNT         6

static struct train_speed_state trainSpeeds[6];


void initTrainSpeeds() {
    char *trains[] = {"45", "47", "48", "49", "50", "51"};
    struct train_speed_state *state;
    unsigned int i, j;
    unsigned int *speeds, *ticks, *stoppingDistances, *stoppingTicks;
    /*
     * Indices 0 to 5 for Trains 45 ... 51
     */
    unsigned int measured_speeds[][MEASUREMENT_TOTAL] = {
        {0, 1171, 1171, 1171, 1809, 2385, 2936, 3517, 3896, 4338, 4744, 5299, 5571, 5694, 5785},
        {0, 1303, 1303, 1303, 1798, 2373, 2892, 3541, 3819, 4338, 4710, 5124, 5158, 5228, 5268},
        {0, 1146, 1146, 1146, 1678, 2350, 2882, 3364, 3782, 4197, 4710, 5193, 5341, 5416, 5538},
        {0, 1275, 1275, 1275, 1704, 2321, 2846, 3426, 3935, 4518, 4993, 5454, 5498, 5538, 5700},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 429, 429, 429, 819, 1138, 1530, 1890, 2240, 2797, 3335, 4127, 4802, 5743, 6586}
    };

    unsigned int measured_ticks[][MEASUREMENT_TOTAL] = {
        {0, 3982, 3892, 3892, 2578, 1955, 1588, 1326, 1197, 1075, 983, 880, 837, 819, 806},
        {0, 3578, 3578, 3578, 2594, 1965, 1612, 1350, 1221, 1075, 990, 910, 904, 892, 885},
        {0, 4068, 4068, 4068, 2779, 1984, 1618, 1386, 1233, 1111, 990, 898, 873, 861, 842},
        {0, 3657, 3657, 3657, 2736, 2009, 1637, 1361, 1185, 1032, 934, 855, 848, 842, 818},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 10880, 10880, 10880, 5696, 4097, 3047, 2467, 2082, 1667, 1398, 1130, 971, 812, 708}
    };

    unsigned int measured_stopping_distances[][MEASUREMENT_TOTAL] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 17, 17, 17, 26, 31, 39, 48, 54, 66, 64, 72, 79, 83, 82},
        {0, 16, 16, 16, 20, 28, 36, 44, 50, 58, 62, 72, 80, 87, 88},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 15, 15, 15, 33, 40, 45, 54, 58, 65, 70, 78, 85, 90, 89},
        {0, 3, 4, 5, 8, 12, 18, 26, 34, 45, 56, 73, 89, 114, 137}
    };

    unsigned int measured_stopping_ticks[][MEASUREMENT_TOTAL] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 206, 206, 206, 265, 271, 336, 331, 347, 375, 352, 400, 405, 356, 386},
        {0, 208, 208, 208, 275, 323, 339, 365, 388, 390, 396, 435, 436, 433, 461},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 193, 193, 193, 285, 311, 343, 358, 362, 351, 370, 395, 412, 399, 389},
        {0, 123, 123, 123, 124, 156, 160, 230, 252, 298, 295, 343, 416, 456, 497}
    };



    for (i = 0; i < TRAIN_COUNT; ++i) {
        state = &trainSpeeds[i];
        state->train = trains[i];
        speeds = measured_speeds[i];
        ticks = measured_ticks[i];
        stoppingDistances = measured_stopping_distances[i];
        stoppingTicks = measured_stopping_ticks[i];
        for (j = 0; j < MEASUREMENT_TOTAL; ++j) {
            state->ticks[j] = ticks[j];
            state->speed[j] = speeds[j];
            state->stoppingDistances[j] = stoppingDistances[j];
            state->stoppingTicks[j] = stoppingTicks[j];
        }
    }
}


bool isValidTrainId(unsigned int tr) {
    bool valid;

    valid = false;
    switch (tr) {
        case 45:
        case 47:
        case 48:
        case 49:
        case 50:
        case 51:
            valid = true;
            break;
    }
    return valid;
}


unsigned int getTrainVelocity(unsigned int tr, unsigned int sp) {
    unsigned int id;

    if (sp > 14) {
        error("getTrainVelocity: Speed out of range.");
        return 0;
    }

    switch (tr) {
        case 45:
            id = 0;
            break;
        case 47:
            id = 1;
            break;
        case 48:
            id = 2;
            break;
        case 49:
            id = 3;
            break;
        case 50:
            id = 4;
            break;
        case 51:
            id = 5;
            break;
        default:
            error("getTrainVelocity: Unknown train.");
            return 0;
    }

    return trainSpeeds[id].speed[sp];
}


unsigned int getTransitionDistance(unsigned int tr, unsigned int startsp, unsigned int destsp, unsigned int ticks) {
    unsigned int curTicks, totalTime, distance;

    if (!isValidTrainId(tr)) {
        return 0;
    }

    curTicks = Time();
    totalTime = getTransitionTicks(tr, startsp, destsp);
    distance = trainSpeeds[tr].stoppingDistances[startsp];
    return (distance / totalTime) * (curTicks - ticks);
}


unsigned int getTransitionTicks(unsigned int tr, unsigned int startsp, unsigned int destsp) {
    unsigned int totalTicks;

    if (!isValidTrainId(tr)) {
        return 0;
    }

    totalTicks = trainSpeeds[tr].stoppingTicks[startsp];
    totalTicks *= ((float)(MAX(startsp, destsp) - MIN(startsp, destsp))) / MAX(startsp, destsp);
    return totalTicks;
}
