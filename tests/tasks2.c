/*
 * Generic tests for finish times and create chaining.
 */
#include <utasks.h>
#include <term.h>
#include <stdio.h>
#include <k_syscall.h>
#include <string.h>
#include <kernel.h>
#include <syscall.h>

#define SEND_SIZE 15
#define REPLY_SIZE 22

void sender() {
    char sendBuffer[16];
    char recvBuffer[10];

    int replyLen = Send(1, &sendBuffer, SEND_SIZE, &recvBuffer, 10);

    if(replyLen != REPLY_SIZE) {
        printf("Incorrect replyLen of %d", replyLen);
    }

    debugf("sender exiting");
}

void receiver() {
    char recvBuffer[20];
    char replBuffer[25];
    int tid;

    int sendLen = Receive(&tid, &recvBuffer, 20);

    if(sendLen != SEND_SIZE) {
        printf("Incorrect sendLen of %d", sendLen);
    }

    int replyResult = Reply(tid, &replBuffer, REPLY_SIZE);

    if(replyResult != 0) {
        printf("Incorrect replyResult of %d", replyResult);
    }

    debugf("receiver exiting");
}

int main() {
    int status;
    uint32_t tid;

    boot();

    sys_create(5, sender, &tid);
    sys_create(6, receiver, &tid);

    kernel_main();
    puts("Done");
    return 0;
}
