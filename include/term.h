#ifndef __TERM__
#define __TERM__
#include <bwio.h>
#include <stdio.h>
#include <vargs.h>
#ifndef BUFFEREDIO
#define printf(format, ...)      bwprintf(IO, format, __VA_ARGS__)
#define puts(str)                bwputstr(IO, str)
#else
#define printf(format, ...)      sprintf(format, __VA_ARGS__)
#define puts(str)                putstr(str)
#endif

/* ANSI color codes */
#define CLEAR                    0
#define BOLD                     1
#define ITALICS                  3
#define UNDERLINE                4
#define BLACK                    30
#define RED                      31
#define GREEN                    32
#define YELLOW                   33
#define BLUE                     34
#define MAGENTA                  35
#define CYAN                     36
#define WHITE                    37

#define restore_cursor()         puts("\033[u")
#define save_cursor()            puts("\033[s")
#define move_cursor(x, y)        printf("\033[%d;%dH", y, x)
#define set_scroll(top, btm)     printf("\033[%d;%dr", top, btm)
#define scroll_up(x)             printf("\033[%dS", x)
#define scroll_down(x)           printf("\033[%dT", x)
#define backspace()              puts("\033[D \033[D")
#define move_cur_up(x)           printf("\033[%dA", x)
#define move_cur_down(x)         printf("\033[%dB", x)
#define move_cur_left(x)         printf("\033[%dD", x)
#define move_cur_right(x)        printf("\033[%dC", x)
#define erase_screen()           puts("\033[2J\033[0;0H")
#define erase_line()             puts("\033[2K")
#define clear_screen()           erase_screen()
#define begin_color(x)           printf("\033[%dm", x)
#define close_color()            puts("\033[0m")
#define newline()                puts("\r\n")

#define TERMINAL_WIDTH           120
#define TERMINAL_HEIGHT          43
#define LEFT_HALF                0
#define RIGHT_HALF               TERMINAL_WIDTH / 2
#define TOP_HALF                 0
#define BOTTOM_HALF              TERMINAL_HEIGHT / 2

void initDebug();
void debug(const char*);

#endif /* __TERM__ */
