/*
 * rps.c - Rock-Paper-Scissors user tasks
 * RockPaperScissors
 *    - Allows the user to start a game of rock paper scissors
 *
 * rps_client, rps_player
 *    - Sends request to the server to play a game.
 *    - RPL_BLK waiting for server to respond with a request for its choice.
 *    - Quits when it loses or the other client loses.
 *
 * rps_server
 *    - Accepts sign up requests from clients, when two are in its queue, it
 *      replies to each of the tasks asking for thier choices.
 *    - RPL_BLK waiting for the tasks to reply with their choice.
 *    - When both replies have been received, it replies to each client with
 *      the result.
 *    - Never quits.
 */
#include <term.h>
#include <syscall.h>
#include <rps.h>
#include <random.h>
#include <shell.h>
#include <server.h>
#include <uart.h>


#define RPS_SERVER   "RPS_SERVER"


static void RPSServer() {
    /* server plays the game of Rock-Paper-Scissors */
    int callee, errno, diff;
    int player1, player2;
    char *p1_name, *p2_name;
    unsigned int i;
    GameMessage res, req;
    GameMessage __free_queue[64];
    GameMessage *free_queue, *tmp;
    GameMessageQueue signup_queue;
    int p1_choice, p2_choice;
    char *choice_names[] = {"ROCK", "PAPER", "SCISSORS"};

    player1 = -1;
    player2 = -1;
    p1_choice = -1;
    p2_choice = -1;
    p1_name = NULL;
    p2_name = NULL;
    signup_queue.head = NULL;
    signup_queue.tail = NULL;

    /* create a free queue of messages that can be used */
    for (i = 0; i < 64; ++i) {
        __free_queue[i].next = (i == 0) ? NULL : free_queue;
        free_queue = &__free_queue[i];
    }

    /* register with the name server */
    RegisterAs(RPS_SERVER);

    while (Receive(&callee, &req, sizeof(req))) {
        switch(req.type) {
            case QUIT:
                /* remove the respective player from the game */
                if (player2 == callee || player1 == callee) {
                    if (callee == player2) {
                        player2 = -1;
                    } else {
                        player1 = -1;
                    }

                    res.type = QUIT;
                    errno = Reply(callee, &res, sizeof(res));

                    /* pop from the queue if one exists and reply */
                    if (signup_queue.head != NULL) {
                        tmp = signup_queue.head;
                        signup_queue.head = tmp->next;

                        if (player1 < 0) {
                            player1 = tmp->tid;
                        } else {
                            player2 = tmp->tid;
                        }

                        if (player1 >= 0 && player2 >= 0) {
                            res.type = tmp->type;
                            errno = Reply(player1, &res, sizeof(res));
                            errno = Reply(player2, &res, sizeof(res));
                        }
                    }
                }
                break;
            case SIGNUP:
                /* add a player to the game only if there aren't two players */
                if (player1 < 0 || player2 < 0) {
                    if (player1 < 0) {
                        player1 = callee;
                    } else if (player2 < 0) {
                        player2 = callee;
                    }

                    if (player1 >= 0 && player2 >= 0) {
                        res.type = req.type;
                        errno = Reply(player1, &res, sizeof(res));
                        errno = Reply(player2, &res, sizeof(res));
                    }
                } else {
                    /* two players are signed up, just add the player to the queue */
                    tmp = free_queue;
                    free_queue = free_queue->next;
                    tmp->next = NULL;
                    tmp->tid = callee;
                    tmp->type = SIGNUP;
                    if (signup_queue.head == NULL) {
                        signup_queue.head = tmp;
                        signup_queue.tail = tmp;
                    } else {
                        signup_queue.tail->next = tmp;
                        signup_queue.tail = tmp;
                    }
                }
                break;
            case PLAY:
                /* if received both choices, play them and reply in turn */
                if (player1 == -1 || player2 == -1) {
                    /* respond that a player quit on them */
                    res.type = PLAYER_QUIT;
                    errno = Reply(callee, &res, sizeof(res));
                } else if (callee == player1) {
                    p1_choice = req.d0;
                    p1_name = req.name;
                } else if (callee == player2) {
                    p2_choice = req.d0;
                    p2_name = req.name;
                } else {
                    /* player that is not in the current game is attempting to play */
                    res.type = NOT_PLAYING;
                    errno = Reply(callee, &res, sizeof(res));
                    break;
                }

                if (p1_choice >= ROCK && p2_choice >= ROCK) {
                    /* compare the hands that were dealt */
                    diff = p1_choice - p2_choice;
                    printf("Player \033[35m%s(Task %d)\033[0m throwing %s\r\n", p1_name, player1, choice_names[p1_choice]);
                    printf("Player \033[33m%s(Task %d)\033[0m throwing %s\r\n", p2_name, player2, choice_names[p2_choice]);
                    switch(diff) {
                        case 0:
                            p1_choice = TIE;
                            p2_choice = TIE;
                            change_color(CYAN);
                            puts("Round was a TIE\r\n");
                            end_color();
                            break;
                        case 1:
                        case -2:
                            p1_choice = WIN;
                            p2_choice = LOSE;
                            change_color(GREEN);
                            printf("%s won the round\r\n", p1_name);
                            end_color();
                            break;
                        case 2:
                        case -1:
                            p1_choice = LOSE;
                            p2_choice = WIN;
                            change_color(GREEN);
                            printf("%s won the round\r\n", p2_name);
                            end_color();
                            break;
                    }

                    /* wait for input to confirm the round */
                    printf("Press any key to continue: \r\n");
                    getchar();
                    newline();

                    /* reply to the two players */
                    res.type = p1_choice;
                    errno = Reply(player1, &res, sizeof(res));
                    res.type = p2_choice;
                    errno = Reply(player2, &res, sizeof(res));
                    p1_choice = -1;
                    p2_choice = -1;
                }
                break;
            default:
                error("Server: Error: Unknown request type: %d", req.type);
        }
    }

    /* should never reach here */
    Exit();
}


static void RPSComputer() {
    /* Client plays a game of Rock-Paper-Scissors. */
    unsigned int i;
    int errno;
    int status;
    int rps_server;
    int tid;
    char name[6];
    GameMessage request, result;

    name[5] = '\0';
    for (i = 0; i < 5; ++i) {
        name[i] = (char)random_range(65, 90);  /* generates a random name */
    }

    /* register with the name server */
    rps_server = WhoIs(RPS_SERVER);
    status = TIE;
    tid = MyTid();

    /* signup to play the game */
    request.type = SIGNUP;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));

    while (errno >= 0 && status == TIE) {
        /* as long as there is a tie, make another play */
        request.type = PLAY;
        request.d0 = random_range(ROCK, SCISSORS);
        request.name = name;
        errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
        if (errno < 0) {
            error("Client: Error in send: %d got %d, sending to: %d", tid, errno, rps_server);
        }
        status = result.type;
    }

    /* should always reach here */
    request.type = QUIT;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
    Exit();
}


static void RPSPlayer() {
    int tid;
    int errno;
    int status;
    char ch;
    int rps_server;
    char name[] = "YOU";
    GameMessage request, result;

    /* register with the name server */
    rps_server = WhoIs(RPS_SERVER);
    status = TIE;
    tid = MyTid();

    /* signup to play the game */
    request.type = SIGNUP;
    request.name = name;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
    change_color(BOLD);
    puts("Welcome to Rock-Paper-Scissors\r\n");
    end_color();
    puts("Enter r/R for ROCK, p/P for Paper, s/S for Scissors\r\n");

    while (errno >= 0 && status == TIE) {
        ch = 4;
        while (ch > SCISSORS) {
            /* loop until a valid input is given */
            change_color(BOLD);
            puts("Make your play: ");
            end_color();
            ch = getchar();
            printf("%c\r\n" SAVE_CURSOR, ch);
            switch (ch) {
                case 82:
                case 114:
                    ch = ROCK;
                    break;
                case 80:
                case 112:
                    ch = PAPER;
                    break;
                case 83:
                case 115:
                    ch = SCISSORS;
                    break;
                default:
                    ch = 4;
                    puts("Invalid input.  Choose one of R, P, S\r\n" SAVE_CURSOR);
            }
        }
        request.type = PLAY;
        request.d0 = (int)ch;
        errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
        if (errno < 0) {
            error("Player: Error in send: %d got %d, sending to: %d", tid, errno, rps_server);
        }
        status = result.type;
    }

    /* should always reach here */
    printf("Game Over.  Returning to prompt.\r\n");
    request.type = QUIT;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
    Exit();
}


void RockPaperScissors() {
    unsigned int i;
    bool quit;
    unsigned int tid1, tid2;

    if (WhoIs(RPS_SERVER) < 0) {
        /* check that the RPS Server exists */
        debug("RockPaperScissors called without RPSServer, creating.");
        Create(5, RPSServer);
    }

    quit = false;
    while (quit == false) {
        puts("Enter the number of players (0 - 1): ");
        i = getchar() - '0';
        printf("%d\r\n", i);

        switch(i) {
            case 0:
                tid1 = Create(4, RPSComputer);
                tid2 = Create(4, RPSComputer);
                quit = true;
                break;
            case 1:
                tid1 = Create(4, RPSPlayer);
                tid2 = Create(4, RPSComputer);
                quit = true;
                break;
            default:
                printf("Invalid player count: %d\r\n", i);
        }
    }

    /* force wait before exiting */
    WaitTid(tid1);
    WaitTid(tid2);

    Exit();
}
