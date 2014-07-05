#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <server.h>
#include <ts7200.h>
#include <syscall.h>
#include <k_syscall.h>
#include <logger.h>
#include <clock.h>
#include <stdio.h>
#include <term.h>
#include <utasks.h>
#include <train.h>
#include <random.h>
#include <sensor_server.h>
#include <track_node.h>
#include <track_data.h>
#include <train_task.h>


void go() {
    int result;
    int tr;
    int trtid;
    char buf[4];
    char ch;
    track_edge *edge;
    unsigned int dist;
    track_node track[TRACK_MAX];

    init_track(track);
    printf("Enter Train Number: ");
    gets(IO, buf, 4);
    tr = atoin(buf, &result);

    trtid = TrCreate(6, tr, &track[24].edge[DIR_AHEAD]);
    TrSpeed(trtid, 0);

    while (true) {
        ch = getchar();
        switch (ch) {
            case 's':
                TrSpeed(trtid, 8);
                break;
            case 't':
                TrSpeed(trtid, 0);
                break;
            case 'r':
                TrReverse(trtid);
                break;
            case 'g':
                TrGetLocation(trtid, &edge, &dist);
                printf("train %d at %d after sensor %s\n", trtid, dist, edge->src->name);
                break;
            case 'q':
                turnOffTrainSet();
                Delay(10);
                SigTerm();
                Exit();
                break;
            default:
                break;
        }
    }
}


int main() {
    unsigned int tid;

    boot();

    sys_create(15, NameServer, &tid);
    sys_create(15, ClockServer, &tid);
    sys_create(12, InputServer, &tid);
    sys_create(12, OutputServer, &tid);
    sys_create(13, TimerTask, &tid);
    sys_create(1, go, &tid);
    sys_create(10, SensorServer, &tid);

    kernel_main();

    shutdown();
    return 0;
}
