/*
 * utasks.c - User tasks for second part of kernel
 * First Task
 *    - Bootstraps the other tasks and the clients.
 *    - Quits when it has gone through all the clients.
 *
 * Client
 *    - Sends request to the server to play a game.
 *    - RPL_BLK waiting for server to respond with a request for its choice.
 *    - Quits when it loses or the other client loses.
 *
 * Server
 *    - Accepts sign up requests from clients, when two are in its queue, it
 *      replies to each of the tasks asking for thier choices.
 *    - RPL_BLK waiting for the tasks to reply with their choice.
 *    - When both replies have been received, it replies to each client with
 *      the result.
 *    - Never quits.
 */
#include <term.h>
#include <syscall.h>
#include <utasks.h>
#include <random.h>

#define NUM_CLIENTS  10
#define RPS_SERVER   "RPS_SERVER"


void testTask() {
    printf("Calling task with priority: %d", getCurrentTask()->priority);
    newline();
}


void firstTask() {
    unsigned int tid, i, priority;

    tid = Create(12, NameServer);    /* create the NameServer */
    nameserver_tid = tid;
    tid = Create(11, Server);        /* create the Server */

    for (i = 0; i < NUM_CLIENTS; ++i) {
        priority = random() % 10;
        tid = Create(priority, Client);     /* lowest possible priority because why not */
    }

    /* should always reach here */
    Exit();
}


void Server() {
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

    while(Receive(&callee, &req, sizeof(req))) {
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

                        res.type = tmp->type;
                        errno = Reply(tmp->tid, &res, sizeof(res));
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
                    res.type = PLAY;
                    errno = Reply(callee, &res, sizeof(res));
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
                if (callee == player1) {
                    p1_choice = req.d0;
                    p1_name = req.name;
                } else if (callee == player2) {
                    p2_choice = req.d0;
                    p2_name = req.name;
                }

                if (p1_choice >= ROCK && p2_choice >= ROCK) {
                    /* compare the hands that were dealt */
                    diff = p1_choice - p2_choice;
                    switch(diff) {
                        case 0:
                            p1_choice = TIE;
                            p2_choice = TIE;
                            puts("Round was a TIE");
                            newline();
                            break;
                        case 1:
                        case -2:
                            p1_choice = WIN;
                            p2_choice = LOSE;
                            printf("%s won the round\r\n", p1_name);
                            break;
                        case 2:
                        case -1:
                            p1_choice = LOSE;
                            p2_choice = WIN;
                            printf("%s won the round\r\n", p2_name);
                            break;
                    }

                    /* reply to the two players */
                    res.type = p1_choice;
                    errno = Reply(player1, &res, sizeof(res));
                    res.type = p2_choice;
                    errno = Reply(player2, &res, sizeof(res));
                    p1_choice = -1;
                    p2_choice = -1;

                    /* wait for input to confirm the round */
                    printf("Press any key to continue: \r\n");
                    getchar();
                    newline();
                } else {
                    /* player that is not in the current game is attempting to play */
                }
                break;
            default:
                debugf("Server: Unknown request type: %d", req.type);
        }
    }

    /* should never reach here */
    Exit();
}


void Client() {
    /* Client plays a game of Rock-Paper-Scissors. */
    unsigned int i;
    int errno;
    int status;
    int rps_server;
    int tid;
    char name[6];
    GameMessage request, result;
    char *choice_names[] = {"ROCK", "PAPER", "SCISSORS"};

    name[5] = '\0';
    for (i = 0; i < 5; ++i) {
        name[i] = (char)random_range(65, 90);  /* generates a random name */
    }

    /* register with the name server */
    errno = RegisterAs(name);
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
        printf("Player %s(Task %d) throwing %s\r\n", name, tid, choice_names[request.d0]);
        errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
        if (errno < 0) {
            debugf("Client: Error in send: %d got %d, sending to: %d\r\n", tid, errno, server);
        }
        status = result.type;
    }

    /* should always reach here */
    request.type = QUIT;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
    Exit();
}
