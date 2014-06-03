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

#define FOREVER            for (;;)


void Shell() {
    char ch;
    char buf[80];
    unsigned int i;

    for (i = 0; i < 80; ++i) buf[i] = 0;

    Delay(22);       /* remove after assignment 3 */
    puts("> ");
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
            } else if (strcmp(buf, "rps") == 0) {
                Create(random_range(2, 3), RockPaperScissors);
            }

            puts("> ");
            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
            save_cursor();
            Pass();
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

    Exit();
}
