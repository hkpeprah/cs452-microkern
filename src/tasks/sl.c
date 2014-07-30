/*
 * sl.c - steam locomotive
 */
#include <term.h>
#include <string.h>
#include <clock.h>
#include <util.h>
#include <stdlib.h>
#include <syscall.h>

#define SL_ARRAY_SIZE   15
#define SL_TERM_WIDTH   132

static char *TRAIN1[] = {
    "                         (@@) (  ) (@)  ( )  @@    ()    @     O     @     O      @      ",
    "                    (   )                                                                ",
    "                (@@@@)                                                                   ",
    "             (    )                                                                      ",
    "           (@@@)                                                                         ",
    "        ====        ________                ___________                                  ",
    "    _D _|  |_______/        \\__I_I_____===__|_________|                                 ",
    "    |(_)---  |   H\\________/ |   |        =|___ ___|      _________________             ",
    "   /     |  |   H  |  |     |   |         ||_| |_||     _|                \\_____A       ",
    "  |      |  |   H  |__--------------------| [___] |   =|                        |        ",
    "  | ________|___H__/__|_____/[][]~\\_______|       |   -|                        |       ",
    "  |/ |   |-----------I_____I [][] []  D   |=======|____|________________________|_       ",
    "__/ =| o |=-O=====O=====O=====O \\ ____Y___________|__|__________________________|_      ",
    " |/-=|___|=    ||    ||    ||    |_____/~\\___/          |_D__D__D_|  |_D__D__D_|        ",
    "  \\_/      \\__/  \\__/  \\__/  \\__/      \\_/               \\_/   \\_/    \\_/   \\_/"};


static char *TRAIN2[] = {
    "                         () (@@@) ()  (@@)  ()    (@)    0     @     0     @      0      ",
    "                    (   )                                                                ",
    "                (@@@@)                                                                   ",
    "             (    )                                                                      ",
    "           (@@@)                                                                         ",
    "        ====        ________                ___________                                  ",
    "    _D _|  |_______/        \\__I_I_____===__|_________|                                 ",
    "    |(_)---  |   H\\________/ |   |        =|___ ___|      _________________             ",
    "   /     |  |   H  |  |     |   |         ||_| |_||     _|                \\_____A       ",
    "  |      |  |   H  |__--------------------| [___] |   =|                        |        ",
    "  | ________|___H__/__|_____/[][]~\\_______|       |   -|                        |       ",
    "  |/ |   |-----------I_____I [][] []  D   |=======|____|________________________|_       ",
    "__/ =| o |=-===0=====0====0===0 \\ ____Y___________|__|__________________________|_      ",
    " |/-=|___|=    ||    ||    ||    |_____/~\\___/          |_D__D__D_|  |_D__D__D_|        ",
    "  \\_/      \\==/  \\==/  \\==/  \\==/      \\_/               \\_/   \\_/    \\_/   \\_/"};



void SteamLocomotive() {
    int maxlen, i;
    int start, end;
    int line_count = 20;
    maxlen = strlen(TRAIN1[0]);
    char buffer[line_count * maxlen];

    start = -1;
    end = 0;
    for (i = SL_TERM_WIDTH - 1; i >= -1 * maxlen; --i) {
        /* through each iteration as the train is moved from the right side of the
           screen to the left side, we construct the train within the buffer before
           displaying it to avoid race conditioning */
        char *line;
        int j, bufIndex;
        if (i > 0) {
            end = (end >= maxlen ? end : end + 1);
            start = 0;
        } else {
            ++start;
        }

        bufIndex = 0;
        for (j = 0; j < SL_ARRAY_SIZE; j++) {
            line = (i % 2 == 0 ? TRAIN1[j] : TRAIN2[j]);
            buffer[bufIndex++] = '\033';
            buffer[bufIndex++] = '[';
            buffer[bufIndex++] = '2';
            buffer[bufIndex++] = 'K';
            buffer[bufIndex++] = '\033';
            buffer[bufIndex++] = '[';
            if (i > 0) {
                if (i >= 10) {
                    buffer[bufIndex++] = (MAX(i, 0) / 10) + '0';
                }
                buffer[bufIndex++] = (MAX(i, 0) % 10) + '0';
            } else {
                buffer[bufIndex++] = '0';
            }
            buffer[bufIndex++] = 'C';
            int k;
            k = (i > 0 ? 0 : start);
            for (; k < MIN(end, maxlen); ++k) {
                buffer[bufIndex++] = line[k];
            }
            buffer[bufIndex++] = '\033';
            buffer[bufIndex++] = '[';
            buffer[bufIndex++] = 'E';
        }
        buffer[bufIndex++] = '\033';
        buffer[bufIndex++] = '[';
        buffer[bufIndex++] = '1';
        buffer[bufIndex++] = '5';
        buffer[bufIndex++] = 'F';
        buffer[bufIndex++] = '\0';
        puts(buffer);
        Delay(20);
        bufIndex = 0;
    }

    /* reset the cursor to the bottom of the screen */
    printf(MOVE_CUR_DOWN SAVE_CURSOR, 15);
    Exit();
}
