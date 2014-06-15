#ifndef __TERM__
#define __TERM__
#include <bwio.h>
#include <stdio.h>
#include <vargs.h>
#include <syscall.h>
#include <train.h>

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

/* Note: ## __VA_ARGS__ is only valid with GNU CPP */
#ifndef BUFFEREDIO
#define printf(format, ...)      bwprintf(IO, format, ## __VA_ARGS__)
#define puts(str)                bwputstr(IO, str)
#define getchar()                bwgetc(IO)
#define putchar(ch)              bwputc(IO, ch)
#define trgetchar()              bwgetc(TRAIN)
#define trputs(str)              trbwputs(str)
#define trnputs(str, n)          trnbwputs(str, n)
#define trputch(ch)              trbwputc(ch)
#else
#define putchar(ch)              Putc(IO, ch)
#define getchar()                Getc(IO)
#define printf(format, ...)      bufprintf(IO, format, ## __VA_ARGS__)
#define puts(str)                bufputstr(IO, str)
#define trgetchar()              Getc(TRAIN)
#define trputs(str)              bufputstr(TRAIN, str)
#define trnputs(str, n)          Putcn(TRAIN, str, n)
#define trputch(ch)              Putc(TRAIN, ch)
#endif
#define kprintf(format, ...)     bwprintf(IO, format, ## __VA_ARGS__)
#define kputstr(str)             bwputstr(IO, str)
#if DEBUG
#define kdebug(format, ...)      {                               \
        bwputstr(IO, SAVE_CURSOR);                               \
        bwprintf(IO, SET_SCROLL, TOP_HALF, BOTTOM_HALF - 1);     \
        bwprintf(IO, MOVE_CURSOR, BOTTOM_HALF - 1, 0);           \
        bwputstr(IO, NEW_LINE);                                  \
        bwprintf(IO, format, ## __VA_ARGS__);                    \
        bwprintf(IO, SET_SCROLL, BOTTOM_HALF + 1, TERMINAL_HEIGHT);   \
        bwprintf(IO, MOVE_CURSOR, BOTTOM_HALF + 1, 0);                \
        bwputstr(IO, RESTORE_CURSOR);                                 \
    }
#define error(format, ...)       debugc(format, RED, ## __VA_ARGS__)
#define notice(format, ...)      debugc(format, CYAN, ## __VA_ARGS__)
#else
#define kdebug(format, ...)
#define notice(format, ...)
#define error(format, ...)
#endif
#define move_to_debug()          (save_cursor(), set_scroll(TOP_HALF, BOTTOM_HALF - 1), move_cursor(0, BOTTOM_HALF - 1))
#define return_to_term()         (set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT), move_cursor(0, BOTTOM_HALF + 1), restore_cursor())

#define RESTORE_CURSOR           "\033[u"
#define SAVE_CURSOR              "\033[s"
#define MOVE_CURSOR              "\033[%d;%dH"
#define RESET_SCROLL             "\033[;r"
#define SET_SCROLL               "\033[%d;%dr"
#define SCROLL_UP                "\033[%dS"
#define SCROLL_DOWN              "\033[%dT"
#define BACKSPACE                "\b \b"
#define MOVE_CUR_UP              "\033[%dF"
#define MOVE_CUR_DOWN            "\033[%dE"
#define MOVE_CUR_LEFT            "\033[%dD"
#define MOVE_CUR_RIGHT           "\033[%dC"
#define ERASE_SCREEN             "\033[2J\033[0;0H"
#define ERASE_LINE               "\033[2K"
#define CHANGE_COLOR             "\033[%dm"
#define NEW_LINE                  "\r\n"
#define SET_WINDOW               "\033]2;\"%s\"ST"

#define restore_cursor()         puts(RESTORE_CURSOR)
#define save_cursor()            puts(SAVE_CURSOR)
#define move_cursor(x, y)        printf(MOVE_CURSOR, y, x)
#define set_scroll(top, btm)     printf(SET_SCROLL, top, btm)
#define reset_scroll()           puts(RESET_SCROLL)
#define scroll_up(x)             printf(SCROLL_UP, x)
#define scroll_down(x)           printf(SCROLL_DOWN, x)
#define backspace()              puts(BACKSPACE)
#define move_cur_up(x)           printf(MOVE_CUR_UP, x)
#define move_cur_down(x)         printf(MOVE_CUR_DOWN, x)
#define move_cur_left(x)         printf(MOVE_CUR_LEFT, x)
#define move_cur_right(x)        printf(MOVE_CUR_RIGHT, x)
#define erase_screen()           puts(ERASE_SCREEN)
#define erase_line()             puts(ERASE_LINE)
#define erase_line_forward()     puts("\033[0K")
#define clear_screen()           erase_screen()
#define change_color(x)          printf(CHANGE_COLOR, x)
#define end_color()              change_color(0)
#define newline()                puts(NEW_LINE)
#define set_line_wrap(col)       printf("\033[7;%dh", col)
#define set_window(name)         printf(SET_WINDOW, name)

#define TERMINAL_WIDTH           80
#define TERMINAL_HEIGHT          63
#define LEFT_HALF                0
#define RIGHT_HALF               TERMINAL_WIDTH / 2
#define TOP_HALF                 3
#define BOTTOM_HALF              20


void initDebug();
void debug(char*, ...);
void debugc(char*, unsigned int, ...);

#endif /* __TERM__ */
