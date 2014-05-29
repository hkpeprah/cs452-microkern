/*
 * server.c - NameServer et. al
 * Name Server
 *    - Accepts registration and lookup requests from other tasks
 *    - RPL_BLK waiting for a task to send it a request
 *    - After each reply, RPL_BLK again
 */
#include <server.h>
#include <hash.h>
#include <syscall.h>
#include <term.h>


int nameserver_tid = 0;


int RegisterAs(char *name) {
    /* sends a request to the server to register */
    int errno;
    Lookup lookup;

    lookup.type = REGISTER;
    lookup.name = name;
    lookup.tid = MyTid();
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));

    if (errno < 0) {
        debugf("RegisterAs: Error in send: %d got %d, sending to: %d", MyTid(), errno, server);
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
        debugf("WhoIs: Error in send: %d got %d, sending to: %d", MyTid(), errno, nameserver_tid);
        newline();
    }

    return lookup.tid;
}


void NameServer() {
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
            case REGISTER:
                insert_ht(clients, lookup.name, lookup.tid);
            case WHOIS:
                if (exists_ht(clients, lookup.name)) {
                    lookup.tid = lookup_ht(clients, lookup.name);
                } else {
                    lookup.tid = TASK_DOES_NOT_EXIST;
                }
                break;
            default:
                debugf("NameServer: Unknown request made: %d", lookup.type);
        }
        errno = Reply(callee, &lookup, sizeof(lookup));
    }

    /* should never reach here */
    Exit();
}
