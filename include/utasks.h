#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>
#include <server.h>


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
    int type;
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


void firstTask();
void Server();
void Client();
void Player();

#endif /* __UTASKS__ */
