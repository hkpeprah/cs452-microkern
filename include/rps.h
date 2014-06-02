#ifndef __ROCK_PAPER_SCISSORS__
#define __ROCK_PAPER_SCISSORS__

struct __gameMessage;


typedef enum {
    WIN = 0,
    LOSE,
    TIE,
    PLAYER_QUIT,
    NOT_PLAYING
} RoundResult;


typedef enum {
    SIGNUP = 0,
    PLAY,
    QUIT
} GameStatus;


typedef enum {
    ROCK = 0,
    PAPER,
    SCISSORS
} Shape;


typedef struct __gameMessage {
    int type : 8;
    int d0;
    int d1;
    char *name;
    unsigned int tid;
    struct __gameMessage *next;
} GameMessage;


typedef struct {
    GameMessage *head;
    GameMessage *tail;
} GameMessageQueue;


void RockPaperScissors();

#endif /* __ROCK_PAPER_SCISSORS__ */
