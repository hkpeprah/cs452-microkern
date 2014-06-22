#include <train.h>
#include <term.h>
#include <syscall.h>
#include <clock.h>
#include <uart.h>

#define TRAIN_AUX_REVERSE     15
#define TRAIN_AUX_SOLENOID    32
#define TRAIN_AUX_STRAIGHT    33
#define TRAIN_AUX_CURVE       34
#define TRAIN_AUX_GO          96
#define TRAIN_AUX_STOP        97
#define TRAIN_AUX_SNSRESET    192

#define TRAIN_MAX_SPEED       14
#define MAX_TRAINS            8

#define SWITCH_INDEX_TO_ID(x) ((x >= TRAIN_SWITCH_COUNT - 4 ? x + MULTI_SWITCH_OFFSET : x) + 1)
#define SWITCH_ID_TO_INDEX(x) ((unsigned int)(TRAIN_SWITCH_COUNT + MULTI_SWITCH_OFFSET - x) < 4 ? x - MULTI_SWITCH_OFFSET - 1 : x)


static Train_t __trainSet[MAX_TRAINS];
static Switch_t trainSwitches[TRAIN_SWITCH_COUNT];
static Sensor_t trainSensors[TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT];
static Train_t *freeSet;
static Train_t *trainSet;
volatile int BW_MASK = 0xFFFFFFFF;


Train_t *addTrain(unsigned int id) {
    Train_t *tmp;

    if (freeSet != NULL && id > 0 && id <= 80) {
        tmp = trainSet;
        trainSet = freeSet;
        freeSet = freeSet->next;
        trainSet->next = tmp;
        trainSet->id = id;
        return trainSet;
    }

    return NULL;
}


void pollSensors() {
    trputch((char)(TRAIN_MODULE_BASE + TRAIN_MODULE_COUNT));
}


void resetSensors() {
    trputch((char)TRAIN_AUX_SNSRESET);
}

static inline void setTrainSwitchState(int index, int state, char statech) {
    int id = SWITCH_INDEX_TO_ID(index);

    trainSwitches[index].id = id;
    trainSwitches[index].state = statech;

    trbwputc(id);
    trbwputc(state);
}

void clearTrainSet() {
    /* resets the entire state of the train controller */
    unsigned int i, index;
    char straight[] = {13, 19, 21};
    char curved[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 20};

    kdebug("Setting the state of switches.");

    for (i = 0; i < sizeof(straight) / sizeof(straight[0]); ++i) {
        index = straight[i];
        setTrainSwitchState(index, TRAIN_AUX_STRAIGHT, 'S');
    }

    for (i = 0; i < sizeof(curved) / sizeof(curved[0]); ++i) {
        index = curved[i];
        setTrainSwitchState(index, TRAIN_AUX_CURVE, 'C');
    }

    trbwputc(TRAIN_AUX_SOLENOID);
    trbwputc(TRAIN_AUX_SNSRESET);
    trainSet = NULL;
    freeSet = NULL;

    for (i = 0; i < MAX_TRAINS; ++i) {
        __trainSet[i].next = freeSet;
        freeSet = &__trainSet[i];
        freeSet->speed = 0;
        freeSet->aux = 0;
    }

    for (i = 0; i < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT; ++i) {
        trainSensors[i].id = (i % TRAIN_SENSOR_COUNT) + 1;
        trainSensors[i].module = i / TRAIN_SENSOR_COUNT;
    }

    kdebug("Switches set.  Train Controller setup complete.");
}


void turnOnTrainSet() {
    BW_MASK = 0xFFFFFFFF;
    bwputc(COM1, TRAIN_AUX_GO);
}


void turnOffTrainSet() {
    Train_t *tmp;

    trputch(TRAIN_AUX_STOP);

    while (trainSet != NULL) {
        trainSpeed(trainSet->id, 0);
        tmp = freeSet;
        freeSet = trainSet;
        trainSet = trainSet->next;
        freeSet->next = tmp;
    }
}


Train_t *getTrain(unsigned int tr) {
    Train_t *train;
    train = trainSet;
    while (train && train->id != tr) {
        train = train->next;
    }
    return train;
}


Switch_t *getSwitch(unsigned int id) {
    id = SWITCH_ID_TO_INDEX(id);

    if (id < TRAIN_SWITCH_COUNT) {
        return &trainSwitches[id];
    }

    return NULL;
}


Sensor_t *getSensor(char module, unsigned int id) {
    id += (module - 'A');
    if (id < TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT) {
        return &trainSensors[id];
    }
    return NULL;
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

    if ((train = getTrain(tr))) {
        buf[0] = TRAIN_AUX_REVERSE;
        buf[1] = tr;
        trnputs(buf, 2);
        return 0;
    }
    return 1;
}


int trainSwitch(unsigned int sw, int ch) {
    char buf[2];

    buf[0] = 0;
    switch (ch) {
        case 'C':
        case 'c':
            buf[0] = TRAIN_AUX_CURVE;
            break;
        case 'S':
        case 's':
            buf[0] = TRAIN_AUX_STRAIGHT;
            break;
    }

    if (buf[0] != 0  && sw >= 0 && sw <= 255) {
        buf[1] = sw;
        trnputs(buf, 2);
        return 0;
    }
    return 1;
}


void turnOffSolenoid() {
    trputch(TRAIN_AUX_SOLENOID);
}


void trbwputc(char ch) {
    int *data, *flags;
    unsigned int mask;

    flags = (int*)(UART1_BASE + UART_FLAG_OFFSET);
    mask = BW_MASK;

    while (true) {
        trbwflush();
        if (*flags & mask && !(*flags & TXBUSY_MASK || *flags & RXFF_MASK)) {
            data = (int*)(UART1_BASE + UART_DATA_OFFSET);
            *data = ch;
            BW_MASK = CTS_MASK;
            break;
        }
    }
}


void trbwputs(char *str) {
    while (*str) {
        trbwputc(*str++);
    }
}


void trnbwputs(char *str, unsigned int len) {
    while (len-- > 0) {
        trbwputc(*str++);
    }
}


char trbwflush() {
    int *flags, *data;
    char ch;

    flags = (int*)(UART1_BASE + UART_FLAG_OFFSET);
    data = (int*)(UART1_BASE + UART_DATA_OFFSET);

    if (!(*flags & RXFF_MASK) || *flags & RXFE_MASK) {
        return -1;
    }

    ch = *data;
    return ch;
}
