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

#define FOREVER            for (;;)


static int SHELL_EXITED = 0;


void NullTask() {
    /* sits on the kernel passing */
    int lastTime = 0;
    int currentTime = 0;
    int timeNotInNull = 0;

    while (!SHELL_EXITED) {
        lastTime = Time();
        Pass();
        currentTime = Time();

        if (currentTime != lastTime) {
            timeNotInNull += currentTime - lastTime;
            save_cursor();
            move_cursor(RIGHT_HALF, 0);
            change_color(MAGENTA);
            printf("Time Not Idle: %d milliseconds\n", 100 - ((timeNotInNull * 100) / currentTime));
            end_color();
            restore_cursor();
        }
    }

    Exit();
}


void Shell() {
    char ch;
    char buf[80];
    unsigned int i;
    unsigned int tid;
    char *help =
        "sl   - Steam locomotive\r\n"
        "rps  - Play a game of Rock-Paper-Scissors\r\n"
        "?    - Display this help dialog\r\n";
    HashTable commands;
    void *command;

    init_ht(&commands);
    insert_ht(&commands, "rps", (int)RockPaperScissors);
    insert_ht(&commands, "sl", (int)SteamLocomotive);

    for (i = 0; i < 80; ++i) buf[i] = 0;

    puts("\r\nCS452 Real-Time Microkernel (Version 0.1.1001)");
    newline();
    puts("Copyright <c> Max Chen, Ford Peprah.  All rights reserved.\r\n> ");
    save_cursor();

    i = 0;
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
            if (strcmp(buf, "q") == 0 || strcmp(buf, "quit") == 0) {
                /* quit the terminal and stop the kernel */
                break;
            } else if (strcmp(buf, "?") == 0) {
                puts(help);
            } else if ((command = (void*)lookup_ht(&commands, buf))) {
                tid = Create(random_range(2, 3), command);
                WaitTid(tid);
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
    Exit();
}
