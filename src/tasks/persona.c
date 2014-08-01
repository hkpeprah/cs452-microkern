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

static volatile CircularBuffer_string msgQueue;
static volatile CircularBuffer_string *messages = NULL;

static string persona_one[] = {
    "I have the strangest feeling someone is watching me.",
    "I want to get off MR BONE'S WILD RIDE!"
};

static string persona_two[] = {
    "Son: D-dad, I want to get off the train.\nFather: I know son.  We all do."
};

static string persona_three[] = {
    "This train is really clean.",
    "When are we going to get to the station?",
    "I want to ride a faster train."
};

static string persona_four[] = {
    "*call* Johnson, you better get that report on my desk by 10 A.M.",
    "I'm going to miss my meeting at this rate."
};

static string *personas[NUM_OF_PERSONALITIES] = {
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


static int writeMessage(string msg) {
    if (messages == NULL) {
        Log("Creating string message queue for intercom.");
        initcb_string((CircularBuffer_string*)&msgQueue);
        messages = &msgQueue;
        Log("Created string message queue for intercom.");
    }

    if (d((CircularBuffer_string*)messages).remaining > 0) {
        ASSERT(msg != NULL, "Received NULL message to write to intercom");
        return write_string((CircularBuffer_string*)messages, &msg, 1);
    }
    return 0;
}


void Passenger() {
    char **personality;
    int len, index, agitationFactor;

    personality = getPersonality();
    len = sizeof(personality) / sizeof(string);
    index = 0;
    agitationFactor = 0;
    Log("Passenger has %d message types", len);
    while (true) {
        if (agitationFactor > 0 && agitationFactor % len == 0) {
            index++;
        }
        index = MIN(index, len - 1);
        if (personality[index] == NULL) {
            error("Passenger: Tried to write message but was NULL");
        } else {
            writeMessage(personality[index]);
        }
        agitationFactor++;
        Delay(500);
    }
    Exit();
}


static void IntercomCourier() {
    string msg;
    int line;

    line = 5;
    Log("IntercomCourier created with Tid %d", MyTid());
    if (messages == NULL) {
        writeMessage((msg = "Listening to intercom:"));
    }

    while (true) {
        /* prints out the messages in the queue every five seconds */
        if (length_string((CircularBuffer_string*)messages) > 0) {
            read_string((CircularBuffer_string*)messages, &msg, 1);
            if (indexOf('\n', msg) != -1) {
                /* this message prints on two lines */
                if (line <= 1) {
                    line = 5;
                }
                printf(SAVE_CURSOR MOVE_CUR_UP "\033[0K" "%s" RESTORE_CURSOR, line, msg);
                line = (line - 2 <= 0 ? 5 : line - 2);
            } else {
                printf(SAVE_CURSOR MOVE_CUR_UP "\033[0K" "%s" RESTORE_CURSOR, line, msg);
                line = (line - 1 <= 0 ? 5 : line);
            }
        }
        Delay(200);
    }
    Exit();
}


void Intercom() {
    /* intercom reserves five units of screen space to display messages */
    int i;
    int printer;
    char ch;

    printer = -1;
    for (i = 0; i < 5; ++i) {
        printf("\r\n");
    }

    if ((printer = Create(4, IntercomCourier)) >= 0) {
        Log("Created IntercomCourier with Tid %d", printer);
        printf("Press any key to exit: ");
        ch = getchar();
        printf("\r\n");
        Destroy(printer);
    }
    Exit();
}
