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


int nameserver_tid = -1;


int RegisterAs(char *name) {
    /* sends a request to the server to register */
    int errno;
    Lookup lookup;

    if (nameserver_tid < 0) {
        return -1;
    }

    lookup.type = REGISTER;
    lookup.name = name;
    lookup.tid = MyTid();
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));

    if (errno < 0) {
        debugf("RegisterAs: Error in send: %d got %d, sending to: %d\r\n", MyTid(), errno, nameserver_tid);
        return -2;
    }

    return 0;
}


int WhoIs(char *name) {
    /* sends a request to the server to lookup user */
    int errno;
    Lookup lookup;

    if (nameserver_tid < 0) {
        return -1;
    }

    lookup.name = name;
    lookup.type = WHOIS;
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));
    if (errno < 0) {
        debugf("WhoIs: Error in send: %d got %d, sending to: %d\r\n", MyTid(), errno, nameserver_tid);
        return -2;
    }

    return lookup.tid;
}


int UnRegister(char *name) {
    /* sends a request to the server to unregster */
    int errno;
    Lookup lookup;

    if (nameserver_tid < 0) {
        return -1;
    }

    lookup.name = name;
    lookup.type = DELETE;
    errno = Send(nameserver_tid, &lookup, sizeof(lookup), &lookup, sizeof(lookup));

    if (errno < 0) {
        debugf("UnRegister: Error in send: %d got %d, sending to %d\r\n", MyTid(), errno, nameserver_tid);
        return -2;
    }

    return 0;
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
                insert_ht(clients, lookup.name, callee);
                debugf("RegisterAs: %s registered.", lookup.name);
            case WHOIS:
                if (exists_ht(clients, lookup.name)) {
                    lookup.tid = lookup_ht(clients, lookup.name);
                } else {
                    debugf("WhoIs: %s does not exist in lookup table.", lookup.name);
                    lookup.tid = TASK_DOES_NOT_EXIST;
                }
                break;
            case DELETE:
                lookup.tid = lookup_ht(clients, lookup.name);
                if (callee == lookup.tid) {
                    /* only the task that matches the tid can delete itself */
                    delete_ht(clients, lookup.name);
                    debugf("UnRegister: Removed %s from lookup table.", lookup.name);
                    lookup.name = "\0";
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
