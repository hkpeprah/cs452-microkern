#include <train_speed.h>
#include <types.h>
#include <term.h>
#include <stdlib.h>
#include <clock.h>
#include <train_speed_measurements.h>

#define MEASUREMENT_TOTAL   15
#define POW_10_3            1000
#define POW_10_4            10000
#define POW_10_5            100000
#define POW_10_6            1000000
#define POW_10_7            10000000
#define POW_10_8            100000000

static train_speed_state train_states[TRAIN_COUNT] = {{0}};


void initTrainSpeeds() {
    init_train_speeds(train_states);
}


void getTrainIds(int *train_ids) {
    int i;
    for (i = 0; i < TRAIN_COUNT; i++) {
        train_ids[i] = train_states[i].train;
    }
}


static int getTrainId(unsigned int tr) {
    unsigned int id;

    for (id = 0; id < TRAIN_COUNT; ++id) {
        if (train_states[id].train == tr) {
            return id;
        }
    }
    error("getTrainId: Unkown train %d", tr);
    return -1;
}


bool isValidTrainId(unsigned int tr) {
    return getTrainId(tr) != -1;
}


unsigned int getTrainVelocity(unsigned int tr, unsigned int sp) {
    int id;

    if (sp > 14) {
        ASSERT(false, "Failed to get train %d", tr);
    }

    if ((id = getTrainId(tr)) >= 0) {
        return train_states[id].speed[sp];
    }
    ASSERT(false, "Failed to get train %d", tr);
    return 0;
}


unsigned int getStoppingDistance(unsigned int tr, int startsp, int destsp) {
    int id = -1;

    if (!isValidTrainId(tr) || (id = getTrainId(tr)) < 0) {
        ASSERT(false, "Failed to get train %d", tr);
    }

    /* stopping distance is measured in millimeters */
    return train_states[id].stoppingDistances[MAX(startsp, destsp)];
}

unsigned int getTransitionTicks(unsigned int tr, int startsp, int destsp) {
    int id;

    if (!isValidTrainId(tr)) {
        ASSERT(false, "Failed to get train %d", tr);
    }

    if ((id = getTrainId(tr)) < 0) {
        ASSERT(false, "Failed to get train %d", tr);
    }

    return train_states[id].stoppingTicks[MAX(startsp, destsp)];
}


static inline int compute_shortmove(int dist, int a, int b, int c, int d) {
    int a_res = (a * (pow(dist, 3) / POW_10_3) ) / POW_10_5;
    int b_res = ((b * pow(dist, 2)) / POW_10_4);
    int c_res = ((c * dist) / POW_10_3);
    return a_res + b_res + c_res + d;
}


int shortmoves(unsigned int tr, unsigned int speed, int dist) {
    /* TODO: Consider speed as some multiplicative value */
    switch (tr) {
        case 48:
            return compute_shortmove(dist, 59, -11, 889, 58);
        case 51:
        case 50:
        case 49:
            return compute_shortmove(dist, 31, -7, 654, 58);
        case 45:
            return compute_shortmove(dist, 18, -5, 583, 84);
        case 47:
            return compute_shortmove(dist, 35, -8, 760, 60);
        case 59:
        case 53:
            return compute_shortmove(dist, 48, -10, 848, 46);
        case 54:
            return compute_shortmove(dist, 42, -10, 879, 47);
        case 56:
            return compute_shortmove(dist, 59, -12, 937, 47);
    }
    ASSERT(false, "Missing train %d", tr);
    return -1;
}


static inline int compute_shortmove_dist(int ticks, int a, int b, int c, int d) {
    return ((a * pow(ticks, 3)) / pow(10, 6)) + ((b * pow(ticks, 2)) / pow(10, 4)) +
        ((c * ticks) / pow(10, 4)) + d;
}


int shortmoves_dist(uint32_t tr, uint32_t speed, uint32_t ticks) {
    int dist;
    /* TODO: Consider speed as some multiplicative value */
    switch (tr) {
        case 48:
            dist = compute_shortmove_dist(ticks, 27, -70, 17500, -79);
            break;
        case 51:
        case 50:
        case 49:
            dist = compute_shortmove_dist(ticks, -23, 284, -40500, 204);
            break;
        case 45:
            dist = compute_shortmove_dist(ticks, 21, 19, 316, 31);
            break;
        case 47:
            dist = compute_shortmove_dist(ticks, 70, -275, 50600, -79);
            break;
        case 59:
        case 53:
            dist = compute_shortmove_dist(ticks, 30, -28, 8200, 23);
            break;
        case 54:
            dist = compute_shortmove_dist(ticks, 40, -95, 17570, 54);
            break;
        case 56:
            dist = compute_shortmove_dist(ticks, 40, -80, 14707, -44);
            break;
        default:
            dist = ticks;
    }
    return MAX(dist, 0);
}
