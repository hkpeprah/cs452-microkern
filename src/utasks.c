/*
 * utasks.c - User tasks for second part of kernel
 * First Task
 *    - Bootstraps the other tasks and the clients.
 *    - Quits when it has gone through all the clients.
 *
 * Name Server
 *    - Accepts registration and lookup requests from other tasks
 *    - RPL_BLK waiting for a task to send it a request
 *    - After each reply, RPL_BLK again
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
#include <hash.h>
#include <random.h>


static int nameserver_tid;


void testTask() {
    printf("Calling task with priority: %d", getCurrentTask()->priority);
    newline();
}


void firstTask() {
    unsigned int i;

    Create(12, nameServer);    /* create the NameServer */
    Create(11, server);        /* create the Server */

    for (i = 0; i < 2; ++i) {
        Create(1, client);     /* lowest possible priority because why not */
    }

    /* should always reach here */
    printf("First exiting...");
    newline();

    Exit();
}


void nameServer() {
    /* NameServer performs lookups and inserts.  It should never exit. */
    int errno;
    int callee;
    Lookup lookup;
    HashTable __clients;
    HashTable *clients;

    init_ht((clients = &__clients));
    nameserver_tid = MyTid();

    /* loop forever processing requests from clients */
    while (Receive(&callee, &lookup, sizeof(lookup))) {
        switch(lookup.type) {
            case SIGNUP:
                insert_ht(clients, lookup.name, lookup.tid);
            case WHOIS:
                lookup.tid = lookup_ht(clients, lookup.name);
                break;
            default:
                debugf("NameServer: Unknown request made: %d", lookup.type);
        }
        errno = Reply(callee, &lookup, sizeof(lookup));
    }

    /* should never reach here */
    Exit();
}


void server() {
    /* server plays the game of Rock-Paper-Scissors */
    int callee;
    int errno;
    int diff;
    int player1;
    int player2;
    GameResult res;
    GameRequest req;
    int p1_choice, p2_choice;

    player1 = -1;
    player2 = -1;
    p1_choice = -1;
    p2_choice = -1;

    RegisterAs("server");

    while(Receive(&callee, &req, sizeof(req))) {
        switch(req.type) {
            case QUIT:
                /* remove the respective player from the game */
                if (player2 == callee) {
                    player2 = -1;
                } else if (player1 == callee) {
                    player1 = -1;
                }
                res.status = 1;
                errno = Reply(callee, &res, sizeof(res));
                break;
            case PLAY:
                /* if received both choices, play them and reply in turn */
                if (player1 < 0) {
                    player1 = callee;
                } else if (player2 < 0) {
                    player2 = callee;
                }

                if (callee == player1) {
                    p1_choice = req.d0;
                } else if (callee == player2) {
                    p2_choice = req.d0;
                }

                if (p1_choice > 0 && p2_choice > 0) {
                    /* compare the hands that were dealt */
                    diff = p1_choice - p2_choice;
                    switch(diff) {
                        case 0:
                            p1_choice = TIE;
                            p2_choice = TIE;
                            puts("TIE");
                            newline();
                            break;
                        case 1:
                        case -2:
                            p1_choice = WIN;
                            p2_choice = LOSE;
                            break;
                        case 2:
                        case -1:
                            p1_choice = LOSE;
                            p2_choice = WIN;
                            break;
                    }

                    /* reply to the two players */
                    res.status = p1_choice;
                    errno = Reply(player1, &res, sizeof(res));
                    res.status = p2_choice;
                    errno = Reply(player2, &res, sizeof(res));
                    p1_choice = -1;
                    p2_choice = -1;
                }
                break;
            default:
                debugf("Server: Unknown request type: %d", req.type);
        }
    }

    /* should never reach here */
    Exit();
}


void client() {
    /* Client plays a game of Rock-Paper-Scissors. */
    unsigned int i;
    int errno;
    int status;
    int rps_server;
    int tid;
    char name[6];
    GameRequest request;
    GameResult result;
    int choices[] = {ROCK, PAPER, SCISSORS};

    name[5] = '\0';
    for (i = 0; i < 5; ++i) {
        name[i] = (char)random_range(65, 90);  /* generates a random name */
    }

    errno = RegisterAs(name);
    status = TIE;
    rps_server = WhoIs("server");
    tid = MyTid();
    while (status == TIE) {
        request.type = PLAY;
        request.d0 = choices[random() % 3];

        switch(request.d0) {
            case ROCK:
                printf("%d throwing rock\r\n", tid);
                break;
            case PAPER:
                printf("%d throwing paper\r\n", tid);
                break;
            case SCISSORS:
                printf("%d throwing scissors\r\n", tid);
                break;
        }

        errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
        if (errno < 0) {
            printf("Client: Error in send: %d got %d, sending to: %d", tid, errno, server);
            newline();
        }
        status = result.status;
    }

    if (status == WIN) {
        printf("%d won!\r\n", tid);
    } else {
        printf("%d lost!\r\n", tid);
    }

    /* should always reach here */
    request.type = QUIT;
    errno = Send(rps_server, &request, sizeof(request), &result, sizeof(result));
    Exit();
}


int RegisterAs(char *name) {
    /* sends a request to the server to register */
    int errno;
    Lookup lookup;

    lookup.type = SIGNUP;
    lookup.name = name;
    lookup.tid = MyTid();
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));

    if (errno < 0) {
        printf("RegisterAs: Error in send: %d got %d, sending to: %d", MyTid(), errno, server);
        newline();
    }

    return errno;
}


int WhoIs(char *name) {
    /* sends a request to the server to lookup user */
    int errno;
    Lookup lookup;

    lookup.name = name;
    lookup.type = WHOIS;
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));
    if (errno < 0) {
        printf("WhoIs: Error in send: %d got %d, sending to: %d", MyTid(), errno, nameserver_tid);
        newline();
    }

    return lookup.tid;
}
