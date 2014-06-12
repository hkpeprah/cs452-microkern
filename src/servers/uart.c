#include <uart.h>
#include <stdlib.h>
#include <term.h>
#include <server.h>
#include <types.h>
#include <syscall.h>
#include <util.h>
#include <ts7200.h>

#define FIFO_SIZE 16

static int is_tid = -1;
static int os_tid = -1;

void enableUartInterrupts() {
    int *uart2ctrl = (int*) (UART2_BASE + UART_CTLR_OFFSET);
//    *uart2ctrl |= RIEN_MASK | TIEN_MASK;
    *uart2ctrl |= RIEN_MASK;
}

char Getc(int channel) {
    UartRequest_t req;
    char ch;
    int result;

    req.type = READ;
    req.channel = channel;
    req.len = 1;
    req.buf = &ch;

    if (is_tid < 0) {
        is_tid = WhoIs(IS_NAME);
    }

    result = Send(is_tid, &req, sizeof(req), NULL, 0);
    if (result < 0) {
        error("Error on send: %d", result);
    }
    return ch;
}

int Putc(int channel, char ch) {
    UartRequest_t req;

    req.type = WRITE;
    req.channel = channel;
    req.len = 1;
    req.buf = &ch;

    if (os_tid < 0) {
        os_tid = WhoIs(OS_NAME);
    }

    return Send(os_tid, &req, sizeof(req), NULL, 0);
}

static void Uart1RCVHandler() {
    int result;
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = WRITE;
    req.channel = 1;
    req.len = 1;

    for (;;) {
        result = AwaitEvent(UART1_RCV_INTERRUPT, NULL, 0);

        if (result < 0) {
            // bad stuff
            continue;
        }

        result = Send(serverTid, &result, sizeof(result), NULL, 0);
    }
}

static void Uart2RCVHandler() {
    int result;
    char buf[FIFO_SIZE];
    UartRequest_t req = {0};
    int serverTid = MyParentTid();

    req.type = WRITE;
    req.channel = 1;

    for (;;) {
        result = AwaitEvent(UART2_RCV_INTERRUPT, buf, FIFO_SIZE);

        if (result < 0) {
            // bad stuff
            continue;
        }

        req.len = result;
        req.buf = buf;

        result = Send(serverTid, &req, sizeof(req), NULL, 0);
    }
}

static void Uart1XMTHandler() {

}

static void Uart2XMTHandler() {
}


void OutputServer() {
    RegisterAs(OS_NAME);
}

void InputServer() {
    int uart1rcv, uart2rcv;
    int result, sender;

    UartRequest_t req;
    CircularBuffer_t cbuf[2] = {{{0}}};
    UartRequestQueue_t requestQueue[2] = {{0}};
    UartRequestQueue_t *reqQueuep;

    requestQueue[0].tid = -1;
    requestQueue[1].tid = -1;

    result = RegisterAs(IS_NAME);
    if (result < 0) {
        error("InputServer: Error: NameServer returned %d", result);
    }

    uart1rcv = Create (15, Uart1RCVHandler);
    uart2rcv = Create (15, Uart2RCVHandler);

    for (;;) {
        result = Receive(&sender, &req, sizeof(req));

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
                }
                break;
        }
    }
}
