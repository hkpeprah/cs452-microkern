#include <utasks.h>
#include <term.h>
#include <stdio.h>
#include <k_syscall.h>
#include <string.h>
#include <kernel.h>

#define FOREVER            for (;;)
#define FIRST_PRIORITY     15


static void prompt() {
    char ch;
    char user[80];
    char buf[80];
    int loggedIn;
    unsigned int i;

    /* produce the login prompt */
    puts("\r\n==============Login===============\r\n");
    puts("User: ");
    save_cursor();

    loggedIn = 0;
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
            /* newline and check for user authentication */
            newline();
            if (loggedIn == 0) {
                strcpy(user, buf);
                loggedIn = 1;
                puts("Password: ");
            } else if (loggedIn == 1) {
                if (login(user, buf)) {
                    loggedIn = 2;
                    printf("Login successful.  Welcome back %s.\r\nLast login: October 5th, 2011\r\n> ", user);
                } else {
                    puts("Login failed.\r\n");
                    loggedIn = 0;
                    puts("User: ");
                }
            } else if (strcmp(buf, "q") == 0) {
                break;
            } else {
                puts("> ");
            }

            for (i = 0; i < 80; ++i) buf[i] = 0;
            i = 0;
            save_cursor();
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


int main() {
    int status;
    uint32_t tid;

    boot();

    /* create the first user task */
    status = sys_create(FIRST_PRIORITY, firstTask, &tid);
    if (status != 0) {
        /* something went wrong creating the first user task */
        return -1;
    }
    
    kernel_main();

    // should reach here after all work has been done
    puts("Exiting...\r\n");
    return 0;
}
