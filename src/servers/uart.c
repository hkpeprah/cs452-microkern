#include <uart.h>
#include <stdlib.h>
#include <term.h>
#include <logger.h>
#include <server.h>
#include <types.h>
#include <syscall.h>
#include <util.h>
#include <ts7200.h>

#define FIFO_SIZE 8

static int is_tid = -1;
static int os_tid = -1;

int Getcn(int channel, volatile char *buf, int n) {
    UartRequest_t req;

    req.type = READ;
    req.channel = channel;
    req.len = n;
    req.buf = buf;

    if (is_tid < 0) {
        is_tid = WhoIs(IS_NAME);
    }

    return Send(is_tid, &req, sizeof(req), NULL, 0);
}


char Getc(int channel) {
    char ch;
    int result;

    result = Getcn(channel, &ch, 1);
    if (result < 0) {
        error("Error on send: %d", result);
    }
    return ch;
}


int Putcn(int channel, volatile char *buf, int n) {
    UartRequest_t req;

    req.type = WRITE;
    req.channel = channel;
    req.len = n;
    req.buf = buf;

    if (os_tid < 0) {
        os_tid = WhoIs(OS_NAME);
    }

    return Send(os_tid, &req, sizeof(req), NULL, 0);
}


int Putc(int channel, char ch) {
    return Putcn(channel, &ch, 1);
}


static void Uart1RCVHandler() {
    int result;
    char buf;
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = WRITE;
    req.channel = COM1;
    req.len = 1;

    for (;;) {
        result = AwaitEvent(UART1_RCV_INTERRUPT, NULL, 0);

        if (result < 0) {
            // bad stuff
            sys_log_f("U1RCV: AwaitEvent result %d\n", result);
            continue;
        }

        buf = result;
        req.buf = &buf;

        result = Send(serverTid, &req, sizeof(req), NULL, 0);
    }
}


static void Uart2RCVHandler() {
    int result;
    char buf[FIFO_SIZE];
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = WRITE;
    req.channel = COM2;

    for (;;) {
        result = AwaitEvent(UART2_RCV_INTERRUPT, buf, sizeof(buf));

        if (result < 0) {
            // bad stuff
            sys_log_f("U2RCV: AwaitEvent result %d\n", result);
            continue;
        }

        req.len = result;
        req.buf = buf;
        result = Send(serverTid, &req, sizeof(req), NULL, 0);
    }
}


static void Uart1XMTHandler() {
    int result;
    char ch;
    volatile int *uart1data = (int*) (UART1_BASE + UART_DATA_OFFSET);
    volatile int *flags = (int*) (UART1_BASE + UART_FLAG_OFFSET);
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = READ;
    req.channel = COM1;
    req.buf = &ch;
    req.len = 1;

    for (;;) {
        result = AwaitEvent(UART1_XMT_INTERRUPT, NULL, 0);

        if (result < 0) {
            sys_log_f("U1XMT: AwaitEvent result %d\n", result);
            continue;
        }

        // TODO: fix this potential race condition
        // ie. if the interrupt occurs before the check, then nothing
        // is waiting on it and the AwaitEvent call here is skipped but the
        // interrupt is still there and masked (will be triggered on the next AwaitEvent)

        // Clearing the interrupt (even when no task wait) won't work either as an interrupt between
        // the if check and the call to AwaitEvent will result in a never-unblocking
        // await.

        // Essentially, this conditional + await needs to be an atomic kernel operation
        // However, we are fine for now as we are beating the train controller in the race
        // by miles

        // wait for cts to assert
        if (!(*flags & CTS_MASK)) {
            result = AwaitEvent(UART1_MOD_INTERRUPT, NULL, 0);
        }

        result = Send(serverTid, &req, sizeof(req), &req, sizeof(req));

        if (result < 0) {
            sys_log_f("U1XMT: Send result %d\n", result);
            continue;
        }

        *uart1data = ch;
#if LOG
        Log("x1: 0x%x\n", ch);
#endif

        // wait for cts to desert if necessary
        if (*flags & CTS_MASK) {
            result = AwaitEvent(UART1_MOD_INTERRUPT, NULL, 0);

            if (result < 0) {
                sys_log_f("U1XMT: AwaitEvent(2) result %d\n", result);
            }
        }
    }
}


static void Uart2XMTHandler() {
    int result, i;
    char buf[FIFO_SIZE];
    volatile int *uart2data = (int*) (UART2_BASE + UART_DATA_OFFSET);
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = READ;
    req.channel = COM2;
    req.buf = buf;

    for (;;) {
        result = AwaitEvent(UART2_XMT_INTERRUPT, NULL, 0);
        if (result < 0) {
            sys_log_f("U2XMT: AwaitEvent result %d\n", result);
            continue;
        }

        req.len = FIFO_SIZE;

        // reply will be same as req except the len parameter is changed to # bytes
        // written back
        result = Send(serverTid, &req, sizeof(req), &req, sizeof(req));

        if (result < 0) {
            sys_log_f("U2XMT: Send result %d\n", result);
            continue;
        }

        for (i = 0; i < req.len; ++i) {
            *uart2data = buf[i];
        }
    }
}


void OutputServer() {
    int i;
    int uart1xmt, uart2xmt;
    int result, sender;

    UartRequest_t req;
    CircularBuffer_t cbuf[2];
    UartRequestQueue_t requestQueue[2] = {{0}};
    UartRequestQueue_t *reqQueuep;

    requestQueue[0].tid = -1;
    requestQueue[1].tid = -1;

    for (i = 0; i < 2; ++i) {
        initcb(&(cbuf[i]));
    }

    result = RegisterAs(OS_NAME);
    if (result < 0) {
        kerror("OutputServer: Error: NameServer returned %d", result);
        return;
    }

    uart1xmt = Create (15, Uart1XMTHandler);
    uart2xmt = Create (15, Uart2XMTHandler);

    for (;;) {
        result = Receive(&sender, &req, sizeof(req));
        if (result != sizeof(req)) {
            kerror("OutputServer: Error: Incorrect message received");
            continue;
        }

        switch(req.type) {
            case READ:
                if (sender != uart1xmt && sender != uart2xmt) {
                    Reply(sender, NULL, 0);
                    continue;
                }

                if (length(&cbuf[req.channel]) <= 0) {
                    // read request but no available chars, add to queue && dont reply
                    requestQueue[req.channel].tid = sender;
                    requestQueue[req.channel].request = req;
                } else {
                    // has at least one, read and reply
                    result = read(&cbuf[req.channel], req.buf, req.len);
                    req.len = result;
                    Reply(sender, &req, sizeof(req));
                }
                break;

            case WRITE:
                write(&cbuf[req.channel], req.buf, req.len);
                reqQueuep = &(requestQueue[req.channel]);
                if (reqQueuep->tid >= 0) {
                    // waiting task exist and enough characters are on queue, then read and reply
                    result = read(&cbuf[req.channel], reqQueuep->request.buf, reqQueuep->request.len);
                    reqQueuep->request.len = result;
                    Reply(reqQueuep->tid, &(reqQueuep->request), sizeof(reqQueuep->request));
                    reqQueuep->tid = -1;
                }

                Reply(sender, NULL, 0);
                break;
        }
    }
}


void InputServer() {
    int i;
    int uart1rcv, uart2rcv;
    int result, sender;

    UartRequest_t req;
    CircularBuffer_t cbuf[2];
    UartRequestQueue_t requestQueue[2] = {{0}};
    UartRequestQueue_t *reqQueuep;

    requestQueue[0].tid = -1;
    requestQueue[1].tid = -1;

    for (i = 0; i < 2; ++i) {
        initcb(&(cbuf[i]));
    }

    result = RegisterAs(IS_NAME);
    if (result < 0) {
        kerror("InputServer: Error: NameServer returned %d", result);
        return;
    }

    uart1rcv = Create(15, Uart1RCVHandler);
    uart2rcv = Create(15, Uart2RCVHandler);

    for (;;) {
        result = Receive(&sender, &req, sizeof(req));
        if (result != sizeof(req)) {
            kerror("InputServer: Error: Incorrect message received");
            continue;
        }

        switch(req.type) {
            case READ:
                if (length(&cbuf[req.channel]) < req.len) {
                    // read request but not enough chars, add to queue && dont reply
                    requestQueue[req.channel].tid = sender;
                    requestQueue[req.channel].request = req;
                } else {
                    // has enough characters, read and reply
                    read(&cbuf[req.channel], req.buf, req.len);
                    Reply(sender, NULL, 0);
                }
            break;

            case WRITE:
                Reply(sender, NULL, 0);
                if (sender != uart1rcv && sender != uart2rcv) {
                    // only those 2 tasks are allowed to write
                    continue;
                }
                // write to circular buffer, check waiting queue
                write(&cbuf[req.channel], req.buf, req.len);

                reqQueuep = &(requestQueue[req.channel]);

                if (reqQueuep->tid >= 0 && reqQueuep->request.len <= length(&cbuf[req.channel])) {
                    // waiting task exist and enough characters are on queue, then read and reply
                    read(&cbuf[req.channel], reqQueuep->request.buf, reqQueuep->request.len);
                    Reply(reqQueuep->tid, NULL, 0);
                    reqQueuep->tid = -1;
                }
            break;
        }
    }
}
