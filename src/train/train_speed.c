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
        {0, 1193, 1193, 1193, 1739, 2207, 2688, 3263, 3779, 4314, 4768, 5228, 5743, 6111, 6103},
        {0, 429, 429, 429, 819, 1138, 1530, 1890, 2240, 2797, 3335, 4127, 4802, 5743, 6586}
    };

    unsigned int measured_ticks[][MEASUREMENT_TOTAL] = {
        {0, 3982, 3892, 3892, 2578, 1955, 1588, 1326, 1197, 1075, 983, 880, 837, 819, 806},
        {0, 3578, 3578, 3578, 2594, 1965, 1612, 1350, 1221, 1075, 990, 910, 904, 892, 885},
        {0, 4068, 4068, 4068, 2779, 1984, 1618, 1386, 1233, 1111, 990, 898, 873, 861, 842},
        {0, 3657, 3657, 3657, 2736, 2009, 1637, 1361, 1185, 1032, 934, 855, 848, 842, 818},
        {0, 3910, 3910, 3910, 2681, 2113, 1735, 1429, 1234, 1081, 978, 892, 812, 763, 764},
        {0, 10880, 10880, 10880, 5696, 4097, 3047, 2467, 2082, 1667, 1398, 1130, 971, 812, 708}
    };

    unsigned int measured_stopping_distances[][MEASUREMENT_TOTAL] = {
        {0, 150, 150, 150, 230, 300, 360, 450, 483, 537, 616, 686, 756, 787, 868},
        {0, 170, 170, 170, 260, 310, 390, 480, 547, 633, 633, 698, 770, 850, 850},
        {0, 160, 160, 160, 200, 280, 360, 440, 560, 620, 700, 775, 855, 935, 960},
        {0, 90, 90, 90, 160, 240, 310, 380, 510, 530, 597, 676, 757, 838, 842},
        {0, 150, 150, 150, 330, 400, 450, 540, 580, 650, 700, 780, 850, 900, 890},
        {0, 30, 40, 50, 80, 120, 180, 260, 340, 450, 560, 730, 890, 1140, 1370}
    };

    unsigned int measured_stopping_ticks[][MEASUREMENT_TOTAL] = {
        {0, 189, 189, 189, 237, 266, 288, 300, 340, 327, 352, 347, 369, 382, 403},
        {0, 206, 206, 206, 265, 271, 336, 331, 347, 375, 352, 400, 405, 356, 386},
        {0, 208, 208, 208, 275, 323, 339, 365, 388, 390, 396, 435, 436, 433, 461},
        {0, 200, 200, 200, 210, 270, 281, 314, 336, 341, 351, 370, 392, 402, 422},
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


static int getTrainId(unsigned int tr) {
    unsigned int id;

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
            return -1;
    }

    return id;
}


unsigned int getTrainVelocity(unsigned int tr, unsigned int sp) {
    int id;

    if (sp > 14) {
        error("getTrainVelocity: Speed out of range.");
        return 0;
    }

    if ((id = getTrainId(tr)) > 0) {
        return trainSpeeds[id].speed[sp];
    }
    return 0;
}


unsigned int getStoppingDistance(unsigned int tr, int startsp, int destsp) {
    int id;

    if (!isValidTrainId(tr) || (id = getTrainId(tr)) < 0) {
        return 0;
    }

    /* stopping distance is measured in millimeters */
    return trainSpeeds[id].stoppingDistances[MAX(startsp, destsp)];
}


unsigned int getTransitionDistance(unsigned int tr, int startsp, int destsp, int ticks) {
    /* returns the distance in millimeters */
    int id;
    unsigned int totalTime, distance;

    if (!isValidTrainId(tr) || (id = getTrainId(tr)) < 0) {
        return 0;
    }

    /* get the total time it takes to transition between speeds */
    totalTime = getTransitionTicks(tr, startsp, destsp);
    /* determint the stopping distance, which is equivalent to acceleration distance */
    distance = trainSpeeds[id].stoppingDistances[MAX(startsp, destsp)] * 1000;
    /* distance travelled accelerating/decelerating is product of distance multiplied by the time spent
     * travelling, divided by the total time it takes to travel */
    distance = (distance * ticks) / totalTime;
    distance /= 1000;
    /* adjust for triangle */
    distance /= 2;
    return distance;
}


unsigned int getTransitionTicks(unsigned int tr, int startsp, int destsp) {
    int id, totalTicks;

    if (!isValidTrainId(tr)) {
        return 0;
    }

    if ((id = getTrainId(tr)) < 0) {
        return 0;
    }

    totalTicks = trainSpeeds[id].stoppingTicks[MAX(startsp, destsp)];
    return totalTicks;
}


unsigned int shortmoves(unsigned int tr, unsigned int speed, int dist) {
    int delay;
    /* TODO: Consider speed as some multiplicative value */
    switch (tr) {
        case 48:
            delay = (7 * dist * dist * dist) + (9486000 * dist) + 518940000;
            delay -= (13000 * dist * dist);
            delay /= 10000000;
            break;
        case 51:
        case 50:
        case 49:
        case 45:
        case 47:
            delay = (5 * dist * dist * dist) + (8978000 * dist) + 439200000;
            delay -= (11000 * dist * dist);
            delay /= 10000000;
            /* fudging the numbers a bit */
            delay += dist;
            break;
        default:
            delay = dist;
    }
    return delay;
}
