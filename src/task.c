#include "proc.h"

#define TASK_QUEUE_SIZE 24

#define LEFT(i) (i * 2 + 1)
#define RIGHT(i) (i * 2 + 2)

static task_t *taskQueue[TASK_QUEUE_SIZE];

static void heapify() {

}

static task_t *peek() {
    return taskQueue[0];
}

static task_t *pop() {

}

static void insert(task_t *task) {

}


int main() {
    task_t tasks[5];

    int i;
    for(i = 0; i < 5; ++i) {
        tasks[i] = {.taskState=READY, .tid=i, parentTid=-1, priority=1, sp=1};
    }

}
