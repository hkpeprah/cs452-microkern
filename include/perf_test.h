#ifndef __PERF_TEST_H__
#define __PERF_TEST_H__

typedef struct {
    int msg[16];
} PerfTestMsg_t;


void initPerf();
void perfGenesisTask();

#endif /* __PERF_TEST_H__ */
