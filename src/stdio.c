/*
 * stdio.c - Standard I/O functions
 */
#include <stdio.h>
#include <string.h>
#include <ts7200.h>
#include <types.h>
#include <vargs.h>
#include <bwio.h>
#include <uart.h>
#include <term.h>
#define __INT             1
#define __HEX             2
#define __STR             3
#define __UINT            4
#define __CHAR            5
#define __LONG            6


int atod(const char ch) {
    /*
     * returns the integer value of the digit
     */
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
	} else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
	return -1;
}


int ctod(const char ch) {
    /*
     * unlike atod, only accepts numbers
     * returns the integer value of the digit
     */
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    return -1;
}


int atoi(const char *source, int *status) {
    /*
     * parses a c-string and interprets the contents as integer
     */
    unsigned int base = 10, num = 0, i = 0;
    int digit;
    *status = 1;
    while(source[i]) {
        digit = atod(source[i]);
        if (digit > base || digit < 0) {
            *status = 0;
            break;
        }
        num = num * base + digit;
        ++i;
    }

    return num;
}


int atoin(const char *source, int *status) {
    /*
     * like atoi but only accepts digits
     */
    unsigned int base = 10, num = 0, i = 0;
    int digit;
    *status = 1;
    while(source[i]) {
        digit = ctod(source[i]);
        if (digit > base || digit < 0) {
            *status = 0;
            break;
        }
        num = num * base + digit;
        ++i;
    }

    return num;
}


void itoa(int num, char *buf) {
    /*
     * converts an integer into the string representation
     * stores result into buf
     */
    if (num < 0) {
        num = -num;
        *buf++ = '-';
    }
    uitoa((unsigned int)num, 10, buf);
}


void uitoa(unsigned int num, unsigned int base, char *buf) {
    /*
     * converts an unsigned integer into the string representation
     * stores result into buf
     */
    int n = 0;
    int digit;
    int d = 1;

    while ((num / d) >= base) {
        d *= base; /* Extract powers of 10 for digits */
    }

    while (d != 0) {
        digit = num / d;  /* extract digit */
        num %= d;         /* extract remainder */
        d = d / base;

        if (n || digit > 0 || d == 0) {
            *buf++ = digit + (digit < 10 ? '0' : 'a' - 10);
            ++n;
        }
    }
    *buf = '\0';
}


int sscanformatted(const char *input, const char *fmt, va_list va) {
    char ch;
    int conv;
    char *tmp;
    unsigned int i;
    unsigned int base;
    unsigned int conversion_type;
    unsigned int nread;
    unsigned int nassigned;
    unsigned int len = strlen(input);
    char buffer[len];
    char *buf = buffer;

    nread = 0;
    nassigned = 0;

    while (true) {
        if (!(ch = *fmt++)) {
            break;
        } else if (len <= 0) {
            return -1;
        } else if (isspace(ch)) {
            while (*fmt && isspace(*fmt)) {
                *fmt++;
            }
            continue;
        }

        if (ch != '%') {
            if (*input != ch || len <= 0) {
                return -1;
            }
            input++;
            nread++;
            continue;
        }

        nread++;
        ch = *fmt++;

        switch (ch) {
            case 'd':
                conversion_type = __INT;
                base = 10;
                break;
            case 'X':
            case 'x':
            case 'p':
                conversion_type = __HEX;
                base = 16;
                break;
            case 's':
                conversion_type = __STR;
                break;
            case 'i':
                conversion_type = __INT;
                base = 10;
                break;
            case 'u':
                conversion_type = __UINT;
                base = 10;
                break;
            case 'c':
                conversion_type = __CHAR;
                break;
            case 'l':
                conversion_type = __LONG;
                break;
            default:
                return -1;
        }

        while (isspace(*input)) {
            ++input;
            --len;
        }

        if (*input == 0) {
            return -1;
        } else if (conversion_type == __CHAR) {
            buf[0] = *input++;
            buf[1] = 0;
            --len;
        } else {
            for (i = 0; !isspace(*input) && ((buf[i] = *input++)); ++i) {
                --len;
            }
            buf[i] = 0;
        }

        switch (conversion_type) {
            case __INT:
                *va_arg(va, int*) = atoin(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __CHAR:
                *va_arg(va, char*) = buf[0];
                break;
            case __UINT:
                *va_arg(va, unsigned int*) = (unsigned int)atoin(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __HEX:
                *va_arg(va, unsigned int*) = (unsigned int)atoi(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __STR:
                tmp = va_arg(va, char*);
                while ((*tmp++ = *buf++));
                break;
        }
        nassigned++;
    }

    while (*input && isspace(*input)) {
        input++;
        --len;
    }

    if (len != 0) return -1;
    return nassigned;
}


int sscanf(const char *src, const char *fmt, ...) {
    va_list va;
    int retval;

    va_start(va, fmt);
    retval = sscanformatted(src, fmt, va);
    va_end(va);

    return retval;
}


char *gets(int channel, char *buf, uint32_t len) {
    int ch;
    uint32_t nread;

    nread = 0;
    while ((ch = Getc(channel))) {
        if (ch == LF || ch == CR || ch == EOF) {
            Putc(IO, '\n');
            break;
        } else if (ch == BS || ch == '\b') {
            if (nread > 0) {
                if (nread < len) {
                    buf--;
                }
                nread--;
                bufputstr(IO, "\b \b");
            }
        } else {
            if (nread < len) {
                *buf++ = ch;
            }
            nread++;
            Putc(IO, ch);
        }
    }
    *buf = 0;
    return buf;
}


void bufputstr(int channel, char *str) {
    Putcn(channel, str, strlen(str));
}


int format(const char *fmt, va_list va, char *buffer) {
    char ch;
    unsigned int i;
    unsigned int len = 14;
    char *tmp;
    char convert_buf[len];

    for (i = 0; i < len; ++i) {
        convert_buf[i] = '\0';
    }

    i = 0;
    while ((ch = *fmt++)) {
        if (ch != '%') {
            buffer[i++] = ch;
        } else {
            ch = *(fmt++);
            switch (ch) {
                case '0':
                    break;
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    ch = ch - '1';
                    break;
            }

            switch (ch) {
                case 'c':
                    buffer[i++] = va_arg(va, char);
                    break;
                case 's':
                    tmp = va_arg(va, char*);
                    while (*tmp) {
                        buffer[i++] = *(tmp++);
                    }
                    break;
                case 'u':
                    uitoa(va_arg(va, unsigned int), 10, convert_buf);
                    break;
                case 'd':
                    itoa(va_arg(va, int), convert_buf);
                    break;
                case 'x':
                    uitoa(va_arg(va, unsigned int), 16, convert_buf);
                    break;
                case '%':
                    buffer[i++] = '%';
                    break;
            }

            if (*convert_buf != '\0') {
                /* if convert buffer is non-empty, we converted something */
                tmp = convert_buf;
                do {
                    buffer[i++] = *tmp;
                } while (*(++tmp) != '\0');
                *convert_buf = '\0';
            }
        }
    }

    buffer[i] = '\0';
    return i;
}


int formatas(const char *fmt, char *buffer, ...) {
    int len = 0;
    va_list va;
    va_start(va, buffer);
    len = format(fmt, va, buffer);
    va_end(va);
    return len;
}


void printformatted(int channel, char *fmt, va_list va) {
    char buffer[VARG_BUF_LEN];
    unsigned int len;

    len = format(fmt, va, buffer);
    Putcn(channel, buffer, len);
}


void bufprintf(int channel, char *fmt, ...) {
    int len;
    char buffer[VARG_BUF_LEN];
    va_list va;

    va_start(va, fmt);
    len = format(fmt, va, buffer);
    va_end(va);

    Putcn(channel, buffer, len);
}
