/*
 * sl.c - staem locomotive
 */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define TERMINAL_WIDTH  272
#define MAX(a, b)       (a > b ? a : b)
#define MIN(a, b)       (a < b ? a : b)


static char *TRAIN1[] = {
    "                         (@@) (  ) (@)  ( )  @@    ()    @     O     @     O      @      ",
    "                    (   )                                                                ",
    "                (@@@@)                                                                   ",
    "             (    )                                                                      ",
    "           (@@@)                                                                         ",
    "        ====        ________               ___________                                  ",
    "    _D _|  |_______/        \\__I_I_____===__|_________|                                 ",
    "    |(_)---  |   H\\________/ |   |        =|___ ___|      _________________             ",
    "   /     |  |   H  |  |     |   |         ||_| |_||     _|                \\_____A       ",
    "  |      |  |   H  |__--------------------| [___] |   =|                        |        ",
    "  | ________|___H__/__|_____/[][]~\\_______|       |    -|                        |       ",
    "  |/ |   |-----------I_____I [][] []  D   |=======|____|________________________|_       ",
    "__/ =| o |=-O=====O=====O=====O \\ ____Y___________|__|___________________________|_      ",
    " |/-=|___|=    ||    ||    ||    |_____/~\\___/          |_D__D__D_|  |_D__D__D_|        ",
    "    \\_/      \\__/  \\__/  \\__/  \\__/          \\_/               \\_/   \\_/     \\_/   \\_/"};


static char *TRAIN2[] = {
    "                         () (@@@) ()  (@@)  ()    (@)    0     @     0     @      0      ",
    "                    (@@@@)                                                               ",
    "                (@)                                                                      ",
    "             (@@)                                                                        ",
    "           (@)                                                                           ",
    "        ====        ________               ___________                                  ",
    "    _D _|  |_______/        \\__I_I_____===__|_________|                                 ",
    "    |(_)---  |   H\\________/ |   |        =|___ ___|      _________________             ",
    "   /     |  |   H  |  |     |   |         ||_| |_||     _|                \\_____A       ",
    "  |      |  |   H  |__--------------------| [___] |   =|                        |        ",
    "  | ________|___H__/__|_____/[][]~\\_______|       |    -|                        |       ",
    "  |/ |   |-----------I_____I [][] []  D   |=======|____|________________________|_       ",
    "__/ =| o |=-===0=====0====0===0 \\ ____Y___________|__|___________________________|_      ",
    " |/-=|___|=    ||    ||    ||    |_____/~\\___/          |_D__D__D_|  |_D__D__D_|        ",
    "    \\_/      \\==/  \\==/  \\==/  \\==/          \\_/               \\_/   \\_/     \\_/   \\_/"};


void SteamLocomotive() {
    char *line;
    int i, j, k;
    int maxlen;
    int start, end;

    maxlen = strlen(TRAIN1[0]);
    start = -1;
    end = 0;
    setbuf(stdout, NULL);

    for (i = TERMINAL_WIDTH - 1; i >= -1 * maxlen; --i) {
        /* determine the start and end indices in the row */
        if (i > 0) {
            end = end >= maxlen ? end : end + 1;
            start = 0;
        } else {
            ++start;
        }

        /* iterate thorugh the rows of the train */
        for (j = 0; j < 15; ++j) {
            line = i % 2 == 0 ? TRAIN1[j] : TRAIN2[j];
            printf("\033[2K\033[%dC", i > 0 ? i : 0);       /* wipe line and move to column */
            k = i > 0 ? 0 : start;
            for (; k < MIN(end, strlen(line)); ++k) {
                if (line[k] == '\\') {
                    if (k > 0 && line[k - 1] != '\\') {
                        printf("%c", '\\');
                        ++k;
                    }
                } else {
                    printf("%c", line[k]);
                }
            }
            printf("\033[E");
        }
        printf("\033[15F");
        usleep(40000);
    }
}


int main() {
    SteamLocomotive();
    return 0;
}
