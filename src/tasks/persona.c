/*
 * persona.c - personalities for the train passengers
 */
#include <persona.h>
#include <random.h>
#include <syscall.h>
#include <stdlib.h>
#include <types.h>
#include <string.h>
#include <term.h>
#include <clock.h>
#include <util.h>

#define NUM_OF_PERSONALITIES   4

DECLARE_CIRCULAR_BUFFER(string);

static CircularBuffer_string *messages = NULL;

static string persona_one[] = {
    "I want to get off MR BONE'S WILD RIDE!",
    "I have the strangest feeling someone is watching me."
};

static string persona_two[] = {
    "Son: D-dad, I want to get off the train.\nFather: I know son.  We all do.",
};

static string persona_three[] = {
    "When are we going to get to the station?",
    "This train is really clean.",
    "I want to ride a faster train."
};

static string persona_four[] = {
    "*call* Johnson, you better get that report on my desk by 10 A.M.",
    "I'm going to miss my meeting at this rate."
};

static string *personas[] = {
    persona_one,
    persona_two,
    persona_three,
    persona_four
};


static char **getPersonality() {
    int n;
    n = random() % NUM_OF_PERSONALITIES;
    return personas[n];
}


static int writeMessage(char *msg) {
    static CircularBuffer_string msgQueue;
    if (messages == NULL) {
        messages = &msgQueue;
    }

    if (length_string(messages) > 0) {
        write_string(messages, &msg, 1);
        return 1;
    }
    return 0;
}


void Passenger() {
    char **personality;
    int len, index;

    personality = getPersonality();
    len = sizeof(personality) / sizeof(personality[0]);

    while (true) {
        index = random() % len;
        writeMessage(personality[index]);
        Delay(500);
    }
    Exit();
}


void IntercomCourier() {
    char *msg;
    int line;

    line = 5;
    while (true) {
        /* prints out the messages in the queue every five seconds */
        if (length_string(messages) > 0) {
            read_string(messages, &msg, 1);
            if (indexOf('\n', msg) != -1) {
                /* this message prints on two lines */
                if (line <= 1) {
                    line = 5;
                }
                printf(SAVE_CURSOR MOVE_CUR_UP "\033[0K" "%s" RESTORE_CURSOR, line, msg);
                line = (line - 2 < 0 ? 5 : line - 2);
            } else {
                printf(SAVE_CURSOR MOVE_CUR_UP "\033[0K" "%s" RESTORE_CURSOR, line, msg);
                line = (line - 1 < 0 ? 5 : line);
            }
        }
        Delay(200);
    }
    Exit();
}


void Intercom() {
    /* intercom reserves five units of screen space to display messages */
    int i, printer;

    for (i = 0; i < 5; ++i) {
        printf("\r\n");
    }

    printer = Create(3, IntercomCourier);
    printf("Press any key to exit: ");
    getchar();
    Destroy(printer);
    Exit();
}
