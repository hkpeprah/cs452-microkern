#ifndef __LOGGER_H__
#define __LOGGER_H__
#include <types.h>

void initLogger();
void printLog(uint32_t start, uint32_t end);
void dumpLog();
int sys_log(const char *buf, int len);
int sys_log_f(const char *fmt, ...);

#endif  /* __LOGGER_H__ */
