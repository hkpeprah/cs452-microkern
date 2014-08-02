/*
 * traincom.c - train communication via passengers/intercom/movement
 */
#include <traincom.h>
#include <random.h>
#include <syscall.h>
#include <stdlib.h>
#include <types.h>
#include <string.h>
#include <term.h>
#include <clock.h>
#include <util.h>
#include <transit.h>

#define STATION_LINES          8
#define MAX_AGITATION_FACTOR   3
#define NUM_OF_PERSONALITIES   4
#define DEFAULT_LINE_COUNT     35

DECLARE_CIRCULAR_BUFFER(string);

static volatile int lines_last_printed = 0;
static volatile CircularBuffer_string msgQueue;
static volatile CircularBuffer_string *messages = NULL;

static string personality_one[] = {
    "Boy: I wish I could ride this train forever!",
    "Boy: This train smells weird.",
    "Boy: I have the strangest feeling someone is watching me.",
    "Boy: Are we there yet?",
    "Boy: I want to get off MR BONE'S WILD RIDE!"
};

static string personality_two[] = {
    "Son: Hey look that bird outside the window.",
    "Son: This train is so big",
    "Father: I like the interior lighting",
    "Son: D-dad, I want to get off the train.\r\n\033[0KFather: I know son.  We all do.",
    "Son: I want to get off MR BONE'S WILD RIDE!"
};

static string personality_three[] = {
    "Girl: This train is really clean.",
    "Girl: This train has really bright lights.",
    "Girl: I want to ride a faster train.",
    "Girl: When are we going to get to the station?",
    "Girl: I want to get off MR BONE'S WILD RIDE!"
};

static string personality_four[] = {
    "Man: I hope we got the Bernstein account.",
    "Man: *call* Johnson, you better get that report on my desk by 10 A.M.",
    "Man: I'm going to miss my meeting at this rate.",
    "Man: That's it, I'm taking this up with Mr Bone's Management!",
    "Man: I want to get off MR BONE'S WILD RIDE!"
};

static string *personalities[NUM_OF_PERSONALITIES] = {
    personality_one,
    personality_two,
    personality_three,
    personality_four
};


static char **getPersonality() {
    int n;
    n = random() % NUM_OF_PERSONALITIES;
    return personalities[n];
}


static int writeMessage(string msg) {
    if (d((CircularBuffer_string*)messages).remaining > 0) {
        ASSERT(msg != NULL, "Received NULL message to write to intercom");
        return write_string((CircularBuffer_string*)messages, &msg, 1);
    }
    return 0;
}


static void Continue() {
    printf("Press any key to exit: ");
    getchar();
    printf("\r\n");
}

static inline void printMessage(int line, int msgNum, string msg) {
    printf(SAVE_CURSOR MOVE_CUR_UP "\033[0K" "(Message %d) %s" RESTORE_CURSOR, line, msgNum, msg);
}


static void IntercomCourier() {
    string msg;
    int num_of_messages, callee, max_line_count;

    Receive(&callee, &max_line_count, sizeof(max_line_count));
    Reply(callee, NULL, 0);

    string history[max_line_count];
    int index = 0;
    num_of_messages = 1;
    max_line_count -= 1;
    while (true) {
        /* prints out the messages in the queue every five seconds */
        if (length_string((CircularBuffer_string*)messages) > 0) {
            read_string((CircularBuffer_string*)messages, &msg, 1);
            if (index == max_line_count - 1) {
                int i;
                for (i = 0; i < max_line_count - 1; ++i) {
                    int index;
                    index = i;
                    if (indexOf('\n', history[i]) >= 0) {
                        ++i;
                    }
                    history[index] = history[index + 1];
                    printMessage(max_line_count - index, num_of_messages - (index - index), history[index]);
                }
                history[i] = msg;
                printMessage(1, num_of_messages, msg);
            } else {
                printMessage(max_line_count - index, num_of_messages, msg);
                history[index++] = msg;
                if (indexOf('\n', msg) >= 0) {
                    if (index == max_line_count - 1) {
                        history[index - 2] = msg;
                        index = max_line_count - 1;
                    } else {
                        index++;
                    }
                }
            }
            num_of_messages++;
        }
        Delay(200);
    }
    Exit();
}


static void StationCourier() {
    int num_lines_printed;

    lines_last_printed = 1;
    num_lines_printed = 1;
    while (true) {
        num_lines_printed = PrintTransitData() + 1;
        lines_last_printed = num_lines_printed;
        Delay(350);
    }
    Exit();
}


void initTransitIntercom() {
    initcb_string((CircularBuffer_string*)&msgQueue);
    messages = &msgQueue;
    notice("Created string message queue for intercom");
}


void Passenger() {
    char **personality;
    int len, index, agitationFactor;

    personality = getPersonality();
    len = MAX(1, (int)sizeof(personality) - 1);
    index = 0;
    agitationFactor = 0;
    while (true) {
        if (agitationFactor > 0 && agitationFactor - len > MAX_AGITATION_FACTOR) {
            agitationFactor = 0;
            index++;
        }
        index = MIN(index, len - 1);
        if (personality[index] == NULL) {
            error("Passenger: Tried to write message but was NULL");
        } else {
            writeMessage(personality[index]);
        }
        agitationFactor++;
        Delay(350);
    }
    Exit();
}


void Intercom() {
    /* intercom reserves five units of screen space to display messages */
    int i, messagePrinter, max_line_count;

    messagePrinter = -1;
    max_line_count = MIN(DEFAULT_LINE_COUNT, TERMINAL_HEIGHT - getTermBottom() - 2) + 1;
    for (i = 0; i < max_line_count - 1; ++i) {
        printf("\r\n");
    }

    if ((messagePrinter = Create(3, IntercomCourier)) >= 0) {
        /* generate the intercom courier if possible */
        Send(messagePrinter, &max_line_count, sizeof(max_line_count), NULL, 0);
        notice("Created IntercomCourier with Tid %d", messagePrinter);
        Continue();
        Destroy(messagePrinter);
    } else {
        Continue();
    }

    Exit();
}


void Probe() {
    /* probs the locations of trains and stations */
    int messagePrinter;

    if ((messagePrinter = Create(3, StationCourier)) >= 0) {
        int i;
        printf("Enter any key to continue: \r\n");
        getchar();
        for (i = 0; i < lines_last_printed; ++i) {
            printf("\r\n");
        }
        Destroy(messagePrinter);
    }
    Exit();
}
