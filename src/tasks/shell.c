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


void Shell() {
    int args[4];
    char ch, buf[80];
    unsigned int i, tid;
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
        "time             -   Get the current formatted time\r\n"
        "add TRAIN        -   Add a train to the track\r\n"
        "?                -   Display this help dialog\r\n";
    char *tmp, *parser[] = {"", "%u", "%u %u", "%u %c", "%c%u"};
    HashTable commands;
    int command, status;
    unsigned int TrainController;
    TrainMessage tr;

    init_ht(&commands);
    insert_ht(&commands, "rps", (int)RockPaperScissors);
    insert_ht(&commands, "go", TRAIN_GO);
    insert_ht(&commands, "stop", TRAIN_STOP);
    insert_ht(&commands, "tr", TRAIN_SPEED);
    insert_ht(&commands, "ax", TRAIN_AUX);
    insert_ht(&commands, "rv", TRAIN_RV);
    insert_ht(&commands, "li", TRAIN_LI);
    insert_ht(&commands, "sw", TRAIN_SWITCH);
    insert_ht(&commands, "ho", TRAIN_HORN);
    insert_ht(&commands, "add", TRAIN_ADD);
    insert_ht(&commands, "wait", TRAIN_WAIT);

    for (i = 0; i < 80; ++i) buf[i] = 0;

    i = 0;
    tr.args = args;
    args[0] = TRAIN_NULL;
    TrainController = WhoIs("TrainHandler");
    Send(TrainController, &tr, sizeof(tr), &status, sizeof(status));
    debug("Shell: Tid %d", MyTid());
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
                            i = 1;
                            break;
                        case TRAIN_SPEED:
                        case TRAIN_AUX:
                            i = 2;
                            break;
                        case TRAIN_SWITCH:
                            i = 3;
                            break;
                        case TRAIN_WAIT:
                            i = 4;
                        default:
                            i = -1;
                    }

                    if (i >= 0) {
                        /* this is a parser command */
                        ++tmp;
                        if (sscanf(tmp, parser[i], &args[1], &args[2], &args[3]) != -1) {
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
            save_cursor();
        } else {
            /* print character to the screen */
            if (i < 79) {
                buf[i] = ch;
            }

            // Log("PutChar called with: %c", ch);
            putchar(ch);
            i++;
            save_cursor();
        }
    }

    SigTerm();
    Exit();
}
