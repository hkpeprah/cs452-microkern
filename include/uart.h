#ifndef __UART_H__
#define __UART_H__

#define OS_NAME "OutputServer"
#define IS_NAME "InputServer"

typedef enum {
    READ,
    WRITE
} UartRequestType_t;

typedef struct {
    UartRequestType_t type;
    unsigned int channel : 1;
    unsigned int len : 15;
    volatile char *buf;
} UartRequest_t;

typedef struct {
    int tid;
    UartRequest_t request;
} UartRequestQueue_t;

int Getcn(int channel, volatile char *buf, int n);
char Getc(int channel);
int Putcn(int channel, volatile char *buf, int n);
int Putc(int channel, char ch);

void OutputServer();
void InputServer();

#endif /* __UART_H__ */
