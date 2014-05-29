#include <ts7200.h>
#include <server.h>
#include <types.h>
#include <syscall.h>
#include <perf_test.h>
#include <term.h>

#define TC4_HIGH 0x80810064
#define TC4_LOW 0x80810060
#define TC4_ENABLE 0x100

#define RECEIVER "ptreceiver"
#define NUM_SENDS 20000

static int MESSAGE_SIZE;

// cache bits 0 (mmu), 2 (dcache), 12 (icache)
static void cacheOn() {
    // enable cache and invalidate it
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "orr r0, r0, #4096\n\t"
        "orr r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "mcr p15, 0, r0, c7, c7, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}
static void cacheOff() {
    // clear cache enable bits
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "bic r0, r0, #4096\n\t"
        "bic r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}

void initPerf() {
    int *tc4enable = (int*) TC4_HIGH;
    *tc4enable = TC4_ENABLE;

#if LARGE
    MESSAGE_SIZE = 16;
    puts("64 byte messages\n");
#else
    MESSAGE_SIZE = 1;
    puts("4 byte messages\n");
#endif

#if CACHE
    // turn cache on
    cacheOn();
    puts("cache on\n");
#else
    cacheOff();
    puts("cache off\n");
#endif
}

// div 983 for milliseconds
static uint32_t getTick() {
    int *tc4value = (int*) TC4_LOW;
    return *tc4value;
}

static void startProfile(uint32_t *tick) {
    *tick = getTick();
}

static void endProfile(uint32_t tick, uint32_t *avg, uint32_t count) {
    uint32_t diff = getTick() - tick;
    *avg = ((*avg * count) + diff) / (count + 1);
}

void senderTask() {
    int receiverTid = WhoIs(RECEIVER);

    uint32_t tick = 0;
    uint32_t avg = 0;
    PerfTestMsg_t msg;

    int i;
    for(i = 0; i < NUM_SENDS; ++i) {
        startProfile(&tick);
        int result = Send(receiverTid, &msg, MESSAGE_SIZE, &msg, MESSAGE_SIZE);
        endProfile(tick, &avg, i);
        debugf("sent tid: %d, count: %d, result: %d\n", receiverTid, i, result);
    }

    printf("send->receive->reply average time: %d\n", avg);
    cacheOff();
    Exit();
}

void receiverTask() {
    int senderTid;
    uint32_t tick = 0;
    uint32_t avg = 0;
    PerfTestMsg_t msg;

    Receive(&senderTid, &msg, MESSAGE_SIZE);
    RegisterAs(RECEIVER);
    printf("receiver registered as tid: %d\n", WhoIs(RECEIVER));
    Reply(senderTid, &msg, MESSAGE_SIZE);

    int i;
    for(i = 0; i < NUM_SENDS; ++i) {
        startProfile(&tick);
        int result = Receive(&senderTid, &msg, MESSAGE_SIZE);
        endProfile(tick, &avg, i);
        debugf("received tid: %d, count: %d, result: %d\n", senderTid, i, result);
        Reply(senderTid, &msg, MESSAGE_SIZE);
    }

    printf("receive average time: %d\n", avg);
    Exit();
}

void perfGenesisTask() {
    Create(15, NameServer);
    int senderPriority = 5;
#if SEND_BEFORE_RECEIVE
    puts("send before receive\n");
    int receiverPriority = 4;
#else
    puts("receive before send\n");
    int receiverPriority = 6;
#endif

    PerfTestMsg_t dummy = {{0}};

    int receiverTid = Create(receiverPriority, receiverTask);
    puts("created receiver\n");

    Send(receiverTid, &dummy, MESSAGE_SIZE, &dummy, MESSAGE_SIZE);
    puts("replied from receiver\n");

    Create(senderPriority, senderTask);
    puts("created sender\n");
    Exit();
}

