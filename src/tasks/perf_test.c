#include <ts7200.h>
#include <server.h>
#include <types.h>
#include <syscall.h>
#include <perf_test.h>
#include <term.h>

#define TC4_HIGH    0x80810064
#define TC4_LOW     0x80810060
#define TC4_ENABLE  0x100
#define RECEIVER    "ptreceiver"
#define NUM_SENDS   20000


static int MESSAGE_SIZE;


#if CACHE
static void cacheOn() {
    /*
     * cache bits 0 (mmu), 2 (dcache), 12 (icache)
     * enable cache and invalidate it
     */
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "orr r0, r0, #4096\n\t"
        "orr r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "mcr p15, 0, r0, c7, c7, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}
#else
static void cacheOff() {
    /* clear cache enable bits */
    asm("stmfd sp!, {r0}\n\t"
        "mrc p15, 0, r0, c1, c0, 0\n\t"
        "bic r0, r0, #4096\n\t"
        "bic r0, r0, #4\n\t"
        "mcr p15, 0, r0, c1, c0, 0\n\t"
        "ldmfd sp!, {r0}\n\t");
}
#endif


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
        cacheOn();
        puts("Cache on\n");
    #else
        cacheOff();
        puts("Cache off\n");
    #endif
}


static uint32_t getTick() {
    /* div 983 for milliseconds */
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
    int result;
    uint32_t tick = 0;
    uint32_t avg = 0;
    PerfTestMsg_t msg;

    int i;
    for (i = 0; i < NUM_SENDS; ++i) {
        startProfile(&tick);
        result = Send(receiverTid, &msg, MESSAGE_SIZE, &msg, MESSAGE_SIZE);
        endProfile(tick, &avg, i);
        debugf("sent tid: %d, count: %d, result: %d\n", receiverTid, i, result);
    }

    printf("send->receive->reply average time: %d\n", avg);
    Exit();
}

void receiverTask() {
    int senderTid;
    int result;
    uint32_t tick = 0;
    uint32_t avg = 0;
    PerfTestMsg_t msg;

    Receive(&senderTid, &msg, MESSAGE_SIZE);
    RegisterAs(RECEIVER);
    Reply(senderTid, &msg, MESSAGE_SIZE);

    int i;
    for (i = 0; i < NUM_SENDS; ++i) {
        startProfile(&tick);
        result = Receive(&senderTid, &msg, MESSAGE_SIZE);
        endProfile(tick, &avg, i);
        debugf("Received tid: %d, count: %d, result: %d\n", senderTid, i, result);
        Reply(senderTid, &msg, MESSAGE_SIZE);
    }

    printf("Receive average time: %d\n", avg);
    Exit();
}

void perfGenesisTask() {
    int receiverPriority;
    int senderPriority;
    int receiverTid;

    Create(15, NameServer);

    #if SEND_FIRST
        puts("Send before receive\n");
        receiverPriority = 4;
    #else
        puts("Receive before send\n");
        receiverPriority = 6;
    #endif

    PerfTestMsg_t dummy = {{0}};
    senderPriority = 5;
    receiverTid = Create(receiverPriority, receiverTask);

    puts("Created receiver\n");

    Send(receiverTid, &dummy, MESSAGE_SIZE, &dummy, MESSAGE_SIZE);
    puts("Replied from receiver\n");

    Create(senderPriority, senderTask);
    puts("Created sender\n");
    Exit();
}

