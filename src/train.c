#include <train.h>
#include <term.h>
#include <syscall.h>
#include <clock.h>
#include <uart.h>
#include <stdlib.h>
#include <ts7200.h>

#define TRAIN_AUX_REVERSE     15
#define TRAIN_AUX_SOLENOID    32
#define TRAIN_AUX_STRAIGHT    33
#define TRAIN_AUX_CURVE       34
#define TRAIN_AUX_GO          96
#define TRAIN_AUX_STOP        97
#define TRAIN_AUX_SNSRESET    192

#define MAX_TRAINS            8

#define SWITCH_INDEX_TO_ID(x) ((x >= TRAIN_SWITCH_COUNT - 4 ? x + MULTI_SWITCH_OFFSET : x) + 1)
#define SWITCH_ID_TO_INDEX(x) ((unsigned int)(TRAIN_SWITCH_COUNT + MULTI_SWITCH_OFFSET - x) < 4 ? x - MULTI_SWITCH_OFFSET - 1 : x)


static Switch_t trainSwitches[TRAIN_SWITCH_COUNT];
static Sensor_t trainSensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT];

int sensorToInt(char module, unsigned int id) {
    return (toUpperCase(module) - 'A') * TRAIN_SENSOR_COUNT + id - 1;
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

    debug("Setting the state of switches.");

    for (i = 0; i < TRAIN_SWITCH_COUNT; ++i) {
        trainSwitch(trainSwitches[i].id, SWITCH_CHAR(trainSwitches[i].state));
    }

    turnOffSolenoid();
    resetSensors();
}


void initTrainSet() {
    /* resets the entire state of the train controller */
    unsigned int i, index;
    char straight[] = {13, 14, 19, 21};
    char curved[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17, 18, 20};

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
    bwputc(TRAIN, TRAIN_AUX_STOP);
    Delay(10);
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


void traverseNode(Train_t *train, track_node *node) {
    Switch_t *sw;

    if (train->currentEdge->dest != node) {
        error("Expected dest node %s but received %s", train->currentEdge->dest, node);
    }

    switch(node->type) {
        case NODE_SENSOR:
        case NODE_MERGE:
        case NODE_ENTER:
        case NODE_EXIT:
            train->currentEdge = &(node->edge[DIR_AHEAD]);
            break;

        case NODE_BRANCH:
            sw = getSwitch(node->num);
            train->currentEdge = &(node->edge[sw->state]);
            break;

        case NODE_NONE:
        default:
            error("BAD NODE TYPE");
    }

    train->edgeDistanceMM = 0;
    train->lastDistUpdateTick = Time();
}


int trainSpeed(unsigned int tr, unsigned int sp) {
    char buf[2];
    Train_t *train;

    train = getTrain(tr);
    if (train != NULL && sp <= TRAIN_MAX_SPEED) {
        train->speed = sp;
        buf[0] = sp + train->aux;
        buf[1] = tr;
        trnputs(buf, 2);
        return 0;
    }
    return 1;
}


int trainAuxiliary(unsigned int tr, unsigned int ax) {
    char buf[2];
    Train_t *train;

    if ((train = getTrain(tr)) && ax >= 16 && ax < 32) {
        if (train->aux >= ax) {
            train->aux -= ax;
        } else {
            train->aux = ax;
        }
        buf[0] = train->aux + train->speed;
        buf[1] = tr;
        trnputs(buf, 2);
        return 0;
    }
    return 1;
}


int trainReverse(unsigned int tr) {
    char buf[2];
    Train_t *train;
    unsigned int speed;

    if ((train = getTrain(tr))) {
        speed = train->speed;
        trainSpeed(train->id, 0);
        Delay(speed + 30);
        buf[0] = TRAIN_AUX_REVERSE;
        buf[1] = tr;
        trnputs(buf, 2);
        Delay(speed + 30);
        return 0;
    }
    return 1;
}


int trainSwitch(unsigned int sw, char ch) {
    char buf[2];
    unsigned int ss;

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
            return -1;
    }

    if (buf[0] != 0  && sw >= 0 && sw <= 255) {
        buf[1] = sw;
        trnputs(buf, 2);
        getSwitch(sw)->state = ss;
        return 0;
    }
    return 1;
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
