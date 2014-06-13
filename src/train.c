#include <train.h>
#include <term.h>
#include <syscall.h>
#include <clock.h>

#define TRAIN_AUX_REVERSE     15
#define TRAIN_AUX_SOLENOID    32
#define TRAIN_AUX_STRAIGHT    33
#define TRAIN_AUX_CURVE       34
#define TRAIN_AUX_GO          96
#define TRAIN_AUX_STOP        97
#define TRAIN_AUX_SNSRESET    192

#define TRAIN_MAX_SPEED       14
#define MAX_TRAINS            8


static Train_t __trainSet[MAX_TRAINS];
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


void clearTrainSet() {
    /* resets the entire state of the train controller */
    unsigned int i;
    char buf[2];

    debug("Setting the state of switches.");
    for (i = 0; i < TRAIN_SWITCH_COUNT; ++i) {
        if (i >= TRAIN_SWITCH_COUNT - 4) {
            buf[0] = (i % 2 == 0 ? TRAIN_AUX_CURVE : TRAIN_AUX_STRAIGHT);
            buf[1] = (i + MULTI_SWITCH_OFFSET) - (TRAIN_SWITCH_COUNT - 4);
        } else {
            buf[0] = (i == 13 ? TRAIN_AUX_STRAIGHT : TRAIN_AUX_CURVE);
            buf[1] = i + 1;
        }
        trputs(buf);
    }

    Delay(4);
    turnOffSolenoid();
    trputch(TRAIN_AUX_SNSRESET);
    trainSet = NULL;
    freeSet = NULL;

    for (i = 0; i < MAX_TRAINS; ++i) {
        __trainSet[i].next = freeSet;
        freeSet = &__trainSet[i];
        freeSet->speed = 0;
        freeSet->aux = 0;
    }

    debug("Switches set.  Train Controller setup complete.");
}


void turnOnTrainSet() {
    char buf[2];
    BW_MASK = 0xFFFFFFFF;

    buf[0] = TRAIN_AUX_GO;
    buf[1] = TRAIN_AUX_GO;
    trputs(buf);
}


void turnOffTrainSet() {
    Train_t *tmp;
    char buf[2];

    buf[0] = TRAIN_AUX_STOP;
    buf[1] = TRAIN_AUX_STOP;
    trputs(buf);

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


int trainSpeed(unsigned int tr, unsigned int sp) {
    char buf[2];
    Train_t *train;

    train = getTrain(tr);
    if (train != NULL && sp <= TRAIN_MAX_SPEED) {
        train->speed = sp;
        buf[0] = sp + train->aux;
        buf[1] = tr;
        debug("TrainSpeed: Bytes: %d %d", buf[0], buf[1]);
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
        debug("TrainAuxiliary: Bytes: %d %d", buf[0], buf[1]);
        trputs(buf);
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
        debug("TrainReverse: Bytes: %d %d", buf[0], buf[1]);
        trputs(buf);
        return 0;
    }
    return 1;
}


int trainSwitch(unsigned int sw, int c) {
    char buf[2];

    buf[0] = 0;
    switch (c) {
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
        debug("TrainSwitch: Bytes: %d %d", buf[0], buf[1]);
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
        if (*flags & mask && !(*flags & 0x8 || *flags & 0x40)) {
            data = (int*)(UART1_BASE + UART_DATA_OFFSET);
            *data = ch;
            BW_MASK = CTS_MASK;
            break;
        }
        trbwflush();
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

    if ((*flags & RXFF_MASK) || *flags & RXFE_MASK) {
        ch = *data;
        return ch;
    }

    return -1;
}
