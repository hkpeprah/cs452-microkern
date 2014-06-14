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
#include <sl.h>
#include <hash.h>
#include <utasks.h>
#include <uart.h>

#define FOREVER            for (;;)


volatile int SHELL_EXITED = 0;


void NullTask() {
    /* sits on the kernel passing */
    while (SHELL_EXITED == 0) {
        Pass();
    }

    notice("NullTask: Exiting");
    Exit();
}


void Shell() {
    char ch;
    char buf[80];
    int args[3];
    unsigned int i;
    unsigned int tid;
    char *help =
        "sl               -   Steam locomotive\r\n"
        "rps              -   Play a game of Rock-Paper-Scissors\r\n"
        "go               -   Start the train set\r\n"
        "stop             -   Stop the train set\r\n"
        "tr TRAIN SPEED   -   Set train TRAIN to move at speed SPEED\r\n"
        "sw SWITCH {C, S} -   Set the specified switch to curve or straight\r\n"
        "ax TRAIN INT     -   Run the auxiliary function on the train\r\n"
        "rv TRAIN         -   Reverse the specified train\r\n"
        "li TRAIN         -   Turn on/off the lights on the specified train\r\n"
        "ho TRAIN         -   Signal the horn on the specified train\r\n"
        "cmdtest TRAIN    -   Run through the auxiliary functions supported by the train\r\n"
        "?                -   Display this help dialog\r\n";
    char *parser[] = {"", "%u", "%u %u", "%u %c"};
    char *tmp;
    HashTable commands;
    int command, status;
    unsigned int TrainController;
    TrainMessage tr;

    init_ht(&commands);
    insert_ht(&commands, "rps", (int)RockPaperScissors);
    insert_ht(&commands, "sl", (int)SteamLocomotive);
    insert_ht(&commands, "go", TRAIN_GO);
    insert_ht(&commands, "stop", TRAIN_STOP);
    insert_ht(&commands, "tr", TRAIN_SPEED);
    insert_ht(&commands, "ax", TRAIN_AUX);
    insert_ht(&commands, "rv", TRAIN_RV);
    insert_ht(&commands, "li", TRAIN_LI);
    insert_ht(&commands, "sw", TRAIN_SWITCH);
    insert_ht(&commands, "ho", TRAIN_HORN);
    insert_ht(&commands, "add", TRAIN_ADD);
    insert_ht(&commands, "cmdtest", TRAIN_EVERYTHING);

    for (i = 0; i < 80; ++i) buf[i] = 0;

    puts("\r\nCS452 Real-Time Microkernel (Version 0.1.1001)\r\n");
    puts("Copyright <c> Max Chen, Ford Peprah.  All rights reserved.\r\n> ");
    save_cursor();

    i = 0;
    TrainController = WhoIs("TrainHandler");
    SHELL_EXITED = 0;

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
                save_cursor();
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
            } else if (strcmp(buf, "?") == 0) {
                puts(help);
            } else if (strcmp(buf, "time") == 0) {
                i = Time();
                printf("%d:%d:%d\r\n", i / 6000, (i / 100) % 60, i % 100);
            } else {
                tmp = &buf[i];
                while (!isspace(*tmp) && *tmp) tmp++;
                *tmp = '\0';

                command = lookup_ht(&commands, &buf[i]);
                if (command > 0) {
                    switch (command) {
                        case TRAIN_GO:
                        case TRAIN_STOP:
                            i = 0;
                            break;
                        case TRAIN_RV:
                        case TRAIN_LI:
                        case TRAIN_HORN:
                        case TRAIN_ADD:
                        case TRAIN_EVERYTHING:
                            i = 1;
                            break;
                        case TRAIN_SPEED:
                        case TRAIN_AUX:
                            i = 2;
                            break;
                        case TRAIN_SWITCH:
                            i = 3;
                            break;
                        default:
                            i = -1;
                    }

                    if (i >= 0) {
                        /* this is a parser command */
                        ++tmp;
                        if (sscanf(tmp, parser[i], &args[1], &args[2]) != -1) {
                            args[0] = command;
                            tr.args = args;
                            Send(TrainController, &tr, sizeof(tr), &status, sizeof(status));
                        }
                    } else {
                        /* this command spawns a user task */
                        tid = Create(random_range(2, 3), (void*)command);
                        WaitTid(tid);
                    }
                }
            }

            puts("> ");
            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
            save_cursor();
        } else {
            /* print character to the screen */
            if (i < 79) {
                buf[i] = ch;
            }

            putchar(ch);
            i++;
            save_cursor();
        }
    }

    SHELL_EXITED = 1;
    notice("Shell: Exiting");
    Exit();
}
