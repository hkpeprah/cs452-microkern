/*
 * shell.c - the *bash* shell
 */
#include <shell.h>
#include <term.h>
#include <stdio.h>
#include <string.h>
#include <utasks.h>
#include <syscall.h>

#define FOREVER            for (;;)


void Shell() {
    char ch;
    char buf[80];
    unsigned int i;

    /* produce the login prompt */
    save_cursor();

    for (i = 0; i < 80; ++i) buf[i] = 0;
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
            } else if (strcmp(buf, "play") == 0) {
                /* user can play their own game of Rock-Paper-Scissors */
                Create(1, Player);
            } else {
                puts("> ");
            }

            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
            save_cursor();
            Pass();
        } else {
            /* print character to the screen */
            if (i < 79) {
                buf[i] = ch;
            }

            if (loggedIn == 1) {
                putchar('*');
            } else {
                putchar(ch);
            }

            i++;
            save_cursor();
        }
    }
}
