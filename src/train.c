#include <train.h>
#include <term.h>
#include <syscall.h>
#include <clock.h>

#define TRAIN_AUX_CURVE       34
#define TRAIN_AUX_STRAIGHT    33
#define TRAIN_AUX_SOLENOID    32
#define TRAIN_AUX_REVERSE     15
#define TRAIN_AUX_GO          96
#define TRAIN_AUX_STOP        97
#define TRAIN_AUX_SNSRESET    192

#define TRAIN_MAX_SPEED       14


volatile int BW_MASK = 0xFFFFFFFF;


void clearTrainSet() {
    /* resets the entire state of the train controller */
    unsigned int i;
    char buf[2];

    debug("Setting the state of switches.");
    for (i = 0; i < TRAIN_SWITCH_COUNT; ++i) {
        if (i >= TRAIN_SWITCH_COUNT - 4) {
            buf[0] = (i % 2 == 0 ? TRAIN_AUX_CURVE : TRAIN_AUX_STRAIGHT);
            buf[1] = MULTI_SWITCH_OFFSET + (TRAIN_SWITCH_COUNT - i);
        } else {
            buf[0] = (i == 13 ? TRAIN_AUX_STRAIGHT : TRAIN_AUX_CURVE);
            buf[1] = i + 1;
        }
        trputs(buf);
    }

    Delay(4);
    turnOffSolenoid();
    trputch(TRAIN_AUX_SNSRESET);
    debug("Switches set.  Train Controller setup complete.");
}


void turnOnTrain() {
    BW_MASK = 0xFFFFFFFF;
    trputch(TRAIN_AUX_GO);
    trputch(TRAIN_AUX_GO);
}


void turnOffTrain() {
    trputch(TRAIN_AUX_STOP);
}


int trainSpeed(unsigned int tr, unsigned int sp) {
    char buf[2];

    /* TODO: Check train exists */
    if (sp <= TRAIN_MAX_SPEED) {
        buf[0] = sp;
        buf[1] = tr;
        trputs(buf);
        return 0;
    }
    return 1;
}


int trainAuxiliary(unsigned int tr, unsigned int ax) {
    char buf[2];

    /* TODO: Check train exists */
    buf[0] = ax;
    buf[1] = tr;
    trputs(buf);
    return 0;
}


int trainReverse(unsigned int tr) {
    char buf[2];

    /* TODO: Check train exists */
    buf[0] = TRAIN_AUX_REVERSE;
    buf[1] = tr;
    trputs(buf);
    return 0;
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

    /* TODO: Check switch exists */
    if (buf[0] != '\0') {
        buf[1] = sw;
        trputs(buf);
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
        trbwputc(*str);
        str++;
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
