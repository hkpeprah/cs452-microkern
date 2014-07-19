#include <train.h>
#include <term.h>
#include <syscall.h>
#include <clock.h>
#include <uart.h>
#include <stdlib.h>
#include <ts7200.h>
#include <dispatcher.h>

#define TRAIN_AUX_SOLENOID    32
#define TRAIN_AUX_STRAIGHT    33
#define TRAIN_AUX_CURVE       34
#define TRAIN_AUX_GO          96
#define TRAIN_AUX_STOP        97
#define TRAIN_AUX_SNSRESET    192
#define MAX_TRAINS            8

#define SWITCH_INDEX_TO_ID(x) ((x >= TRAIN_SWITCH_COUNT - 4 ? x + MULTI_SWITCH_OFFSET : x) + 1)
#define SWITCH_ID_TO_INDEX(x) (((unsigned int)(TRAIN_SWITCH_COUNT + MULTI_SWITCH_OFFSET - x) < 4 ? x - MULTI_SWITCH_OFFSET : x) - 1)

static Switch_t trainSwitches[TRAIN_SWITCH_COUNT];
static Sensor_t trainSensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT];


int sensorToInt(char module, unsigned int id) {
    id = (toUpperCase(module) - 'A') * TRAIN_SENSOR_COUNT + id - 1;
    if (id >= TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT) {
        return -1;
    }
    return id;
}


void pollSensors() {
    trputch((char)(TRAIN_MODULE_BASE + TRAIN_MODULE_COUNT));
}


void resetSensors() {
    trputch((char)TRAIN_AUX_SNSRESET);
}


static inline void setTrainSwitchState(int index, int state, int swstate) {
    int id = SWITCH_INDEX_TO_ID(index);
    trainSwitches[index].id = id;
    trainSwitches[index].state = swstate;
}


void setTrainSetState() {
    unsigned int i;
    char trains[] = {45, 46, 47, 48, 49, 50, 51};

    debug("Setting the state of switches.");
    for (i = 0; i < TRAIN_SWITCH_COUNT; ++i) {
        trainSwitch(trainSwitches[i].id, SWITCH_CHAR(trainSwitches[i].state));
    }

    debug("Setting supported trains to speed 0.");
    for (i = 0; i < 7; ++i) {
        trainSpeed(trains[i], 0);
    }

    turnOffSolenoid();
    resetSensors();
}


void initTrainSet() {
    /* resets the entire state of the train controller */
    unsigned int i, index;
    char straight[] = {9, 12, 15, 16, 19, 21};
    char curved[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13, 14, 17, 18, 20};

    for (i = 0; i < sizeof(straight) / sizeof(straight[0]); ++i) {
        index = straight[i];
        setTrainSwitchState(index, TRAIN_AUX_STRAIGHT, DIR_STRAIGHT);
    }

    for (i = 0; i < sizeof(curved) / sizeof(curved[0]); ++i) {
        index = curved[i];
        setTrainSwitchState(index, TRAIN_AUX_CURVE, DIR_CURVED);
    }

    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
        trainSensors[i].id = (i % TRAIN_SENSOR_COUNT) + 1;
        trainSensors[i].module = (i / TRAIN_SENSOR_COUNT) + 'A';
    }

    kdebug("Initialized train set.");
}


void turnOnTrainSet() {
    bwputc(TRAIN, TRAIN_AUX_GO);
}


void turnOffTrainSet() {
    trbwputc(TRAIN_AUX_STOP);
}


Switch_t *getSwitch(unsigned int id) {
    id = SWITCH_ID_TO_INDEX(id);

    if (id < TRAIN_SWITCH_COUNT) {
        return &trainSwitches[id];
    }

    return NULL;
}


Sensor_t *getSensor(char module, unsigned int id) {
    id = MAX(0, id - 1);
    id += ((module - 'A') * TRAIN_SENSOR_COUNT);
    if (id < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT) {
        return &trainSensors[id];
    }
    return NULL;
}


Sensor_t *getSensorFromIndex(unsigned int index) {
    char module;
    unsigned int id;

    module = (index / TRAIN_SENSOR_COUNT) + 'A';
    id = index % TRAIN_SENSOR_COUNT + 1;

    return getSensor(module, id);
}


int trainSwitch(unsigned int sw, char ch) {
    char buf[2];
    unsigned int ss;
    Switch_t *swtch;

    buf[0] = 0;
    switch (ch) {
        case 'C':
        case 'c':
            buf[0] = TRAIN_AUX_CURVE;
            ss = DIR_CURVED;
            break;
        case 'S':
        case 's':
            buf[0] = TRAIN_AUX_STRAIGHT;
            ss = DIR_STRAIGHT;
            break;
        default:
            kerror("Invalid switch character: %c", ch);
            return INVALID_SWITCH_STATE;
    }

    swtch = getSwitch(sw);
    if (buf[0] != 0  && sw >= 0 && sw <= 255) {
        buf[1] = sw;
        trnputs(buf, 2);
        swtch->state = ss;
        printSwitch(swtch->id, (toUpperCase(ch)));
        return 0;
    }
    return INVALID_SWITCH_ID;
}


int trainSpeed(unsigned int tr, unsigned int speed) {
    char buf[2];

    if (speed > TRAIN_MAX_SPEED) {
        return INVALID_SPEED;
    }

    buf[0] = speed;
    buf[1] = tr;
    trnputs(buf, 2);
    return 0;
}


int trainReverse(unsigned int tr) {
    char buf[2];
    buf[0] = TRAIN_AUX_REVERSE;
    buf[1] = tr;
    trnputs(buf, 2);
    return 0;
}


void turnOffSolenoid() {
    trputch(TRAIN_AUX_SOLENOID);
}


void trbwputc(char ch) {
    volatile int *data, *flags;
    flags = (int*)(UART1_BASE + UART_FLAG_OFFSET);

    while (true) {
        if (*flags & CTS_MASK && !(*flags & TXBUSY_MASK || *flags & RXFF_MASK)) {
            data = (int*)(UART1_BASE + UART_DATA_OFFSET);
            *data = ch;
            break;
        }
    }
}


int trbwflush() {
    volatile int *flags, *data;
    char ch;

    flags = (int*)(UART1_BASE + UART_FLAG_OFFSET);
    data = (int*)(UART1_BASE + UART_DATA_OFFSET);

    if (*flags & RXFE_MASK || !(*flags & RXFF_MASK)) {
        return -1;
    }

    ch = *data;
    return ch;
}
