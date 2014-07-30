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
#include <demo.h>
#include <sl.h>

#define FOREVER            for (;;)
#define HELP_MESSAGES      24
#define PROMPT             "\033[34;1m[%d]\033[0m\033[32;1m%s@rtfolks $ \033[0m"


static void print_help() {
    static char *help[] = {
        "rps                         -   Play a game of Rock-Paper-Scissors",
        "go                          -   Start the train set",
        "stop                        -   Stop the train set",
        "tr TRAIN SPEED              -   Set train TRAIN to move at speed SPEED",
        "sw SWITCH {C, S}            -   Set the specified switch to curve or straight",
        "ax TRAIN INT                -   Run the auxiliary function on the train",
        "rv TRAIN                    -   Reverse the specified train",
        "li TRAIN                    -   Turn on/off the lights on the specified train",
        "ho TRAIN                    -   Signal the horn on the specified train",
        "add TRAIN                   -   Add a train to the track",
        "add-at TRAIN SNSR           -   Add a train to the track at the specified sensor",
        "goto TRAIN SNSR             -   Tell train to go to specified sensor",
        "gt TRAIN SNSR dist          -   Tell train to go to specified distance after sensor",
        "mv TRAIN dist               -   Tell train to move specified distance",
        "st TRAIN                    -   Stop the train from routing",
        "rsv TRAIN SNSR1 SNSR2       -   Reserve sensors from SNSR1 to SNSR2 for TRAIN",
        "rls TRAIN SNSR1 SNSR2       -   Release sensors from SNSR1 to SNSR2 if they are assigned to TRAIN",
        "raw BYTE1 BYTE2             -   Sends two raw bytes to the train controller",
        "whoami                      -   Prints the current user",
        "sl                          -   Display animations",
        "clear                       -   Clears the screen",
        "sudo                        -   Run with elevated privileges",
        "help                        -   Display this help dialog",
        "?                           -   Display this help dialog"
    };
    unsigned int i;
    for (i = 1; i < HELP_MESSAGES; ++i) {
        printf("%s\r\n", help[i]);
    }
}


static char *getUsername() {
    static int id = -1;
    static char *names[] = {
        "mqchen",
        "hkpeprah",
        "root",
        "etivrusky"
    };

    if (id == -1) {
        seed(random_seed());
        seed(random());
        id = random() % 4;
    }
    return names[id];
}


static void clear() {
    static char NEWLINE_BUFFER[TERMINAL_HEIGHT + 1] = { [0 ... TERMINAL_HEIGHT] = '\n'};
    NEWLINE_BUFFER[TERMINAL_HEIGHT] = '\0';
    printf("%s" MOVE_CURSOR SAVE_CURSOR, NEWLINE_BUFFER, getTermBottom(), 0);
}


static void sudo() {
    if (strcmp(getUsername(), "root") != 0) {
        printf("sudo: check your privilege\r\n");
        return;
    }
    printf("sudo: running as root.\r\n");
}


static void whoami() {
    printf("%s\r\n", getUsername());
}


void Shell() {
    int args[6];
    char ch, buf[80];
    HashTable commands;
    ControllerMessage_t tr;
    unsigned int TrainController, totalCommands;
    int command, status, i, tid, cmd, exit_status;
    char *tmp, *parser[] = {"", "%u", "%u %u", "%u %c", "%c%u", "%u %c%u", "%u %c%u %u", "%u %c%u %c%u"};

    init_ht(&commands);
    insert_ht(&commands, "rps", (int)RockPaperScissors + NUM_TRAIN_CMD_COMMANDS);
    insert_ht(&commands, "demo", (int)TrainDemo + NUM_TRAIN_CMD_COMMANDS);
    insert_ht(&commands, "sl", (int)SteamLocomotive + NUM_TRAIN_CMD_COMMANDS);
    insert_ht(&commands, "go", TRAIN_CMD_GO);
    insert_ht(&commands, "stop", TRAIN_CMD_STOP);
    insert_ht(&commands, "tr", TRAIN_CMD_SPEED);
    insert_ht(&commands, "ax", TRAIN_CMD_AUX);
    insert_ht(&commands, "rv", TRAIN_CMD_RV);
    insert_ht(&commands, "li", TRAIN_CMD_LI);
    insert_ht(&commands, "sw", TRAIN_CMD_SWITCH);
    insert_ht(&commands, "ho", TRAIN_CMD_HORN);
    insert_ht(&commands, "add", TRAIN_CMD_ADD);
    insert_ht(&commands, "add-at", TRAIN_CMD_ADD_AT);
    insert_ht(&commands, "goto", TRAIN_CMD_GOTO);
    insert_ht(&commands, "gt", TRAIN_CMD_GOTO_AFTER);
    insert_ht(&commands, "st", TRAIN_CMD_GOTO_STOP);
    insert_ht(&commands, "rsv", TRAIN_CMD_RESERVE);
    insert_ht(&commands, "rls", TRAIN_CMD_RELEASE);
    insert_ht(&commands, "mv", TRAIN_CMD_MOVE);
    insert_ht(&commands, "raw", TRAIN_CMD_RAW);

    for (i = 0; i < 80; ++i) buf[i] = 0;

    i = 0;
    totalCommands = 1;
    tr.args = args;
    exit_status = 0;
    TrainController = WhoIs(USER_TRAIN_DISPATCH);
    notice("Shell: Tid %d, User's Controller: %u", MyTid(), TrainController);
    printf(PROMPT, totalCommands, getUsername());
    save_cursor();

    FOREVER {
        ch = getchar();
        if (ch == BS || ch == '\b') {
            /* remove a character from the line */
            if (i > 0) {
                if (i < 80) {
                    buf[--i] = 0;
                } else {
                    i--;
                }
                backspace();
            }
        } else if (ch == CR || ch == LF) {
            i = 0;
            while (isspace(buf[i])) {
                i++;
            }

            newline();
            if (strcmp(buf, "q") == 0 || strcmp(buf, "quit") == 0 || sscanf(buf, "q %d", &exit_status) != -1) {
                /* quit the terminal and stop the kernel */
                newline();
                break;
            } else if (strcmp(buf, "?") == 0 || strcmp(buf, "help") == 0) {
                print_help();
            } else if (strcmp(buf, "clear") == 0) {
                clear();
            } else if (strcmp(buf, "sudo") == 0) {
                sudo();
            } else if (strcmp(buf, "whoami") == 0) {
                whoami();
            } else {
                /* add null terminating character to first available position */
                tmp = &buf[i];
                while (!isspace(*tmp) && *tmp) {
                    tmp++;
                }
                *tmp = '\0';
                /* match the command found between i ... tmp */
                command = lookup_ht(&commands, &buf[i]);
                if (command > 0) {
                    switch (command) {
                        case TRAIN_CMD_GO:
                        case TRAIN_CMD_STOP:
                            cmd = 0;
                            break;
                        case TRAIN_CMD_RV:
                        case TRAIN_CMD_LI:
                        case TRAIN_CMD_HORN:
                        case TRAIN_CMD_ADD:
                        case TRAIN_CMD_GOTO_STOP:
                            cmd = 1;
                            break;
                        case TRAIN_CMD_SPEED:
                        case TRAIN_CMD_AUX:
                        case TRAIN_CMD_MOVE:
                        case TRAIN_CMD_RAW:
                            cmd = 2;
                            break;
                        case TRAIN_CMD_SWITCH:
                            cmd = 3;
                            break;
                        case TRAIN_CMD_ADD_AT:
                        case TRAIN_CMD_GOTO:
                            cmd = 5;
                            break;
                        case TRAIN_CMD_GOTO_AFTER:
                            cmd = 6;
                            break;
                        case TRAIN_CMD_RESERVE:
                        case TRAIN_CMD_RELEASE:
                            cmd = 7;
                            break;
                        default:
                            cmd = -1;
                    }

                    if (cmd >= 0) {
                        /* this is a parser command */
                        ++tmp;
                        if (sscanf(tmp, parser[cmd], &args[1], &args[2], &args[3], &args[4], &args[5]) != -1) {
                            args[0] = command;
                            Send(TrainController, &tr, sizeof(tr), &status, sizeof(status));
                        } else {
                            printf("%s: invalid arguments.\r\n", &buf[i]);
                        }
                    } else {
                        /* this command spawns a user task */
                        tid = Create(random_range(2, 3), (void*)(command - NUM_TRAIN_CMD_COMMANDS));
                        WaitTid(tid);
                    }
                } else if (strlen(&buf[i]) > 0) {
                    printf("%s: command not found.\r\n", &buf[i]);
                }
            }

            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
            totalCommands++;
            printf(PROMPT, totalCommands, getUsername());
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
    SigTerm(exit_status);
    Exit();
}
