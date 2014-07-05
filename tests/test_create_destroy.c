#include <syscall.h>
#include <server.h>
#include <clock.h>
#include <term.h>
#include <utasks.h>
#include <types.h>
#include <kernel.h>
#include <stdio.h>
#include <k_syscall.h>
#include <uart.h>

void Murica() {
    int result;

    // test 1: cannot destroy non-parent
    result = Destroy(WhoIs("BestKorea"));
    printf("Murica tries to destroy BestKorea, expect: %d, received: %d\n", DESTROY_NOT_CHILD, result);
}

void China() {
    printf("China running\n");
    int parent = MyParentTid();
    printf("China will try to send to %d\n", parent);
    printf("China return from send with %d\n", Send(parent, NULL, 0, NULL, 0));
}

void Mao() {
    // creating China at higher priority
    Create(5, China);
    // function end should return to Exit (destroy self)
    // expect china to return from send
    printf("Expect china to return from send with result %d\n", TASK_DESTROYED);
}

void Taiwan() {
    printf("taiwan running\n");
    int sender;
    Receive(&sender, NULL, 0);
    printf("taiwan received from %d\n", sender);
    Reply(sender, NULL, 0);
}

void NorthKorea() {
    int sender, result;
    int child1, child2, child3, child4;

    RegisterAs("BestKorea");

    printf("North korea is best korea\n");
    printf("There is no better korea\n");

    // test 1: cannot destroy non-parent
    printf("Creating Murica which will try to destroy BestKorea\n");
    child1 = Create(5, Murica);

    // test 2: reusing task descriptors but different TID's
    child2 = Create(3, Murica);
    printf("Recreated Murica. Murica 1 tid: %d, Murica 2 tid: %d\n", child1, child2);

    // test 3: destroy child == remove them from ready queue
    child1 = Create(3, Murica);
    child3 = Create(3, China);
    child4 = Create(3, Murica);
    // queue right now is child2, child1, child3, child4
    Destroy(child1);
    Destroy(child2);
    child2 = Create(3, Taiwan);
    printf("Recreated child 2, tid: %d\n", child2);
    Destroy(child4);
    // queue should be China Taiwan

    printf("NK: Expecting China to run next\n");
    Receive(&sender, NULL, 0);
    printf("NK: Expected tid: %d, got tid: %d\n", child3, sender);
    Reply(sender, NULL, 0);

    // test 4: send to destroyed task should give no task exist
    result = Send(child1, NULL, 0, NULL, 0);
    printf("Sending to destroyed task, expected %d, got %d\n", TASK_DOES_NOT_EXIST, result);

    // test 5: task should clear its send queue when destroyed
    Create(4, Mao);

    Destroy(child1);
    Destroy(child2);
    Destroy(child3);
    Destroy(child4);
    while (getchar() != 'q');
    SigTerm();
}

void Jesus() {
    int id;

    id = Create(2, NameServer);
    Destroy(id);

    id = Create(15, NameServer);
    id = Create(15, ClockServer);
    id = Create(12, InputServer);
    id = Create(12, OutputServer);
    id = Create(13, TimerTask);
    id = Create(4, NorthKorea);
}

int main() {
    int tid, result;

    boot();

    result = sys_create(5, Jesus, &tid);

    kernel_main();

    shutdown();
    return 0;
}
