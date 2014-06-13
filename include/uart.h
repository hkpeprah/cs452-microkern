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
    int channel : 1;
    int len : 15;
    char *buf;
} UartRequest_t;

typedef struct {
    int tid;
    UartRequest_t request;
} UartRequestQueue_t;

void enableUartInterrupts();

char Getc(int channel);
int Putc(int channel, char ch);

void OutputServer();
void InputServer();

#endif /* __UART_H__ */
