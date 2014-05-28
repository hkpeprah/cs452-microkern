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


static int nameserver_tid;


void testTask() {
    printf("Calling task with priority: %d", getCurrentTask()->priority);
    newline();
}


void firstTask() {
    


}


void nameServer() {
    /* NameServer performs lookups and inserts.  It should never exit. */
    int error;
    int callee;
    Lookup lookup;
    int msglen;
    HashTable __clients;
    HashTable *clients;

    init_ht((clients = &__clients));

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
        Reply(callee, &lookup, sizeof(lookup));
    }

    /* should never reach here */
    Exit();
}


void server() {
    /* server plays the game of Rock-Paper-Scissors */
    int callee;
    int error;
    int player1;
    int player2;
    Lookup lookup;
    GameResults res;
    GameRequest req;
    int p1_choice, p2_choice;

    player1 = -1;
    player2 = -1;
    p1_choice = -1;
    p2_choice = -1;
    lookup.type = SIGNUP;

    while(Receive(&callee, &req, sizeof(req))) {
        switch(req.type) {
        case QUIT:
            if (player2 == req.d0) {
                player2 = -1;
            } else if (player1 == req.d1) {
                player1 = -1;
            }
            break;
        case SIGNUP:
            if (player1 < 0 || player2 < 0) {
                /* register only if there are currently not to people playing */
                lookup.name = (char*)req.d0;
                lookup.tid = (int)req.d1;
                error = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));
                if (player1 < 0) {
                    player1 = lookup.tid;
                } else {
                    player2 = lookup.tid;
                }
            }
            break;
        case PLAY:
            break;
        }
        /* send a response back to the callee */
    }

    /* should never reach here */
    Exit();
}


void client() {
    /* Client plays a game of Rock-Paper-Scissors. */
    unsigned int i;
    int error;
    int status;
    int server;
    char name[6];
    GameRequest request;
    GameResult result;
    int choices[] = {ROCK, PAPER, SCISSORS};

    name[5] = 0;
    for (i = 0; i < 5; ++i) {
        name[i] = (char)random_range(65, 91);  /* generates a random name */
    }

    /* register and play the game */
    server = RegisterAs(name, MyTid());
    status = TIE;
    while (status == TIE) {
        request.type = PLAY;
        request.d0 = choices[random() % 3];
        error = Send(server, &request, sizeof(request), &result, sizeof(result));
        if (error < 0) {
            debugf("Client: Error in send: %d got %d", MyTid(), error);
        }
        status = result.status;
    }

    /* should always reach here */
    request.type = QUIT;
    request.d0 = MyTid();
    error = Send(server, &request, sizeof(request), &result, sizeof(result));
    Exit();
}


int RegisterAs(char *name, int tid) {
    /* sends a request to the server to register */
    int error;
    int server;
    GameRequest request;
    GameResult result;

    request.type = SIGNUP;
    request.d0 = (int)name;
    request.d1 = tid;

    server = WhoIs("server");
    error = Send(server, &request, sizeof(request), &result, sizeof(result));
    if (error < 0) {
        debugf("RegisterAs: Error in send: %d got %d", MyTid(), error);
    }

    return server;
}


int WhoIs(char *name) {
    /* sends a request to the server to lookup user */
    int error;
    Lookup lookup;

    lookup.name = name;
    lookup.type = WHOIS;
    error = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));
    if (error < 0) {
        debubf("WhoIs: Error in send: %d got %d", MyTid(), error);
    }

    return lookup.tid;
}
