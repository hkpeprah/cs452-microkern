#ifndef __TERM__
#define __TERM__
#include <bwio.h>
#include <stdio.h>
#include <vargs.h>
/* Note: ## __VA_ARGS__ is only valid with GNU CPP */
#ifndef BUFFEREDIO
#define printf(format, ...)      bwprintf(IO, format, ## __VA_ARGS__)
#define puts(str)                bwputstr(IO, str)
#define getchar()                bwgetc(IO)
#define putchar(ch)              bwputc(IO, ch)
#else
#define printf(format, ...)      sprintf(format, ## __VA_ARGS__)
#define puts(str)                putstr(str)
#define getchar()                getch()
#define putchar(ch)              putch(ch)
#endif
#if DEBUG
#define debugf(format, ...)      (move_to_debug(), printf("\r\n"), printf(format, ## __VA_ARGS__), return_to_term())
#define debug(str)               debugf("%s", str)
#else
#define debug(str)
#define debugf(format, ...)
#endif
#define move_to_debug()          (save_cursor(), set_scroll(0, BOTTOM_HALF - 1), move_cursor(0, BOTTOM_HALF - 1))
#define return_to_term()         (set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT), move_cursor(0, BOTTOM_HALF + 1), restore_cursor())
#define error(format, ...)       (change_color(RED), debugf(format, ## __VA_ARGS__), end_color())


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

/* character codes */
#define BS                       9
#define LF                       10
#define CR                       13

#define restore_cursor()         puts("\033[u")
#define save_cursor()            puts("\033[s")
#define move_cursor(x, y)        printf("\033[%d;%dH", y, x)
#define set_scroll(top, btm)     printf("\033[%d;%dr", top, btm)
#define reset_scroll()           puts("\033[;r")
#define scroll_up(x)             printf("\033[%dS", x)
#define scroll_down(x)           printf("\033[%dT", x)
#define backspace()              puts("\b \b")
#define move_cur_up(x)           printf("\033[%dA", x)
#define move_cur_down(x)         printf("\033[%dB", x)
#define move_cur_left(x)         printf("\033[%dD", x)
#define move_cur_right(x)        printf("\033[%dC", x)
#define erase_screen()           puts("\033[2J\033[0;0H")
#define erase_line()             puts("\033[2K")
#define erase_line_forward()     puts("\033[0K")
#define clear_screen()           erase_screen()
#define change_color(x)          printf("\033[%dm", x)
#define end_color()              puts("\033[0m")
#define newline()                puts("\r\n")
#define set_line_wrap(col)       printf("\033[7;%dh", col)
#define set_window(name)         printf("\033]2;\"%s\"ST", name)

#define TERMINAL_WIDTH           120
#define TERMINAL_HEIGHT          63
#define LEFT_HALF                0
#define RIGHT_HALF               TERMINAL_WIDTH / 2
#define TOP_HALF                 0
#define BOTTOM_HALF              20


void initDebug();

#endif /* __TERM__ */
