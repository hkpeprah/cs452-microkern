/*
 * shell.c - the *bash* shell
 */
#include <shell.h>
#include <term.h>
#include <stdio.h>
#include <string.h>
#include <rps.h>
#include <syscall.h>
#include <random.h>
#include <clock.h>
#include <hash.h>
#include <utasks.h>
#include <uart.h>
#include <dispatcher.h>

#define FOREVER            for (;;)
#define HELP_MESSAGES      17

static void print_help() {
    static char *help[HELP_MESSAGES];
    help[0] = "sl                          -   Steam locomotive\r\n";
    help[1] = "rps                         -   Play a game of Rock-Paper-Scissors\r\n";
    help[2] = "go                          -   Start the train set\r\n";
    help[3] = "stop                        -   Stop the train set\r\n";
    help[4] = "tr TRAIN SPEED              -   Set train TRAIN to move at speed SPEED\r\n";
    help[5] = "sw SWITCH {C, S}            -   Set the specified switch to curve or straight\r\n";
    help[6] = "ax TRAIN INT                -   Run the auxiliary function on the train\r\n";
    help[7] = "rv TRAIN                    -   Reverse the specified train\r\n";
    help[8] = "li TRAIN                    -   Turn on/off the lights on the specified train\r\n";
    help[9] = "ho TRAIN                    -   Signal the horn on the specified train\r\n";
    help[10] = "time                        -   Get the current formatted time\r\n";
    help[11] = "add TRAIN SNSR              -   Add a train to the track at specified sensor\r\n";
    help[12] = "goto TRAIN SNSR             -   Tell train to go to specified sensor\r\n";
    help[13] = "goto-after TRAIN SNSR dist  -   Tell train to go to specified distance after sensor\r\n";
    help[14] = "whoami                      -   Prints the current user\r\n";
    help[15] = "help                        -   Display this help dialog\r\n";
    help[16] = "?                           -   Display this help dialog\r\n";
    unsigned int i;

    // TODO: Figure out why can't send sl to be printed....
    for (i = 1; i < 16; ++i) {
        puts(help[i]);
    }
}


static void whoami() {
    static int id = -1;
    char *names[] = {
        "mqchen",
        "hkpeprah",
        "root",
        "etivrusky"
    };

    if (id == -1) {
        seed(Time());
        id = random() % 4;
    }

    printf("%s\r\n", names[id]);
}


void Shell() {
    int args[5];
    char ch, buf[80];
    unsigned int i, tid;
    char *tmp, *parser[] = {"", "%u", "%u %u", "%u %c", "%c%u", "%u %c%u", "%u %c%u %u"};
    HashTable commands;
    int command, status;
    unsigned int TrainController;
    ControllerMessage_t tr;

    init_ht(&commands);
    insert_ht(&commands, "rps", (int)RockPaperScissors);
    insert_ht(&commands, "go", TRM_GO);
    insert_ht(&commands, "stop", TRM_STOP);
    insert_ht(&commands, "tr", TRM_SPEED);
    insert_ht(&commands, "ax", TRM_AUX);
    insert_ht(&commands, "rv", TRM_RV);
    insert_ht(&commands, "li", TRM_LI);
    insert_ht(&commands, "sw", TRM_SWITCH);
    insert_ht(&commands, "ho", TRM_HORN);
    insert_ht(&commands, "add", TRM_ADD);
    insert_ht(&commands, "add-at", TRM_ADD_AT);
    insert_ht(&commands, "goto", TRM_GOTO);
    insert_ht(&commands, "goto-after", TRM_GOTO_AFTER);

    for (i = 0; i < 80; ++i) buf[i] = 0;

    i = 0;
    tr.args = args;
    TrainController = WhoIs(USER_TRAIN_DISPATCH);
    notice("Shell: Tid %d, User's Controller: %u", MyTid(), TrainController);
    puts("> ");
    save_cursor();

    FOREVER {
        ch = getchar();
        if (ch == BS || ch == '\b') {
            /* remove a character from the line */
            if (i > 0) {
                if (i < 80) {
                    buf[i] = 0;
                }
                i--;
                backspace();
            }
        } else if (ch == CR || ch == LF) {
            newline();
            i = 0;
            while (isspace(buf[i])) {
                i++;
            }

            if (strcmp(buf, "q") == 0 || strcmp(buf, "quit") == 0) {
                /* quit the terminal and stop the kernel */
                break;
            } else if (strcmp(buf, "?") == 0 || strcmp(buf, "help") == 0) {
                print_help();
            } else if (strcmp(buf, "time") == 0) {
                i = Time();
                printf("%d:%d:%d\r\n", i / 6000, (i / 100) % 60, i % 100);
            } else if (strcmp(buf, "whoami") == 0) {
                whoami();
            } else {
                tmp = &buf[i];
                while (!isspace(*tmp) && *tmp) tmp++;
                *tmp = '\0';

                command = lookup_ht(&commands, &buf[i]);
                if (command > 0) {
                    switch (command) {
                        case TRM_GO:
                        case TRM_STOP:
                            i = 0;
                            break;
                        case TRM_RV:
                        case TRM_LI:
                        case TRM_HORN:
                        case TRM_ADD:
                            i = 1;
                            break;
                        case TRM_SPEED:
                        case TRM_AUX:
                            i = 2;
                            break;
                        case TRM_SWITCH:
                            i = 3;
                            break;
                        case TRM_ADD_AT:
                        case TRM_GOTO:
                            i = 5;
                            break;
                        case TRM_GOTO_AFTER:
                            i = 6;
                            break;
                        default:
                            i = -1;
                    }

                    if (i >= 0) {
                        /* this is a parser command */
                        ++tmp;
                        if (sscanf(tmp, parser[i], &args[1], &args[2], &args[3], &args[4]) != -1) {
                            args[0] = command;
                            Send(TrainController, &tr, sizeof(tr), &status, sizeof(status));
                        }
                    } else {
                        /* this command spawns a user task */
                        tid = Create(random_range(2, 3), (void*)command);
                        WaitTid(tid);
                    }
                } else {
                    printf("%s: command not found.\r\n", &buf[i]);
                }
            }

            puts("> ");
            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
        } else {
            /* print character to the screen */
            if (i < 79) {
                buf[i] = ch;
            }
            putchar(ch);
            i++;
        }
        save_cursor();
    }
    SigTerm();
    Exit();
}
