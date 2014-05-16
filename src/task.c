#include <stdio.h>

#include "task.h"

#define TASK_QUEUE_SIZE 24

#define h_left(x) (x * 2 + 1)
#define h_right(x) (x * 2 + 2)
#define h_parent(x) ( (x - 1) / 2 )

static task_t *taskQueue[TASK_QUEUE_SIZE] = {0};
static int taskQueueSize = 0;

static void h_swap(task_t **a, task_t **b) {
    task_t *tmp = *a;
    *a = *b;
    *b = tmp;
}

static task_t *h_peek() {
    return taskQueue[0];
}

static task_t *h_pop() {
    // save current top value
    task_t *result = taskQueue[0];
    // swap with the last value
    h_swap(&taskQueue[0], &taskQueue[--taskQueueSize]);

    // push the last value down
    int i = 0;
    for(;;) {
        // compute left/right children
        int left = h_left(i), right = h_right(i);

        if(left < taskQueueSize && taskQueue[left]->priority >= taskQueue[right]->priority) {
            // left children exists and is >= than right

            // current >= left -> current >= right, no point in swapping, return
            if(taskQueue[i]->priority >= taskQueue[left]->priority) {
                return result;
            }

            // otherwise, make the swap 
            h_swap(&taskQueue[i], &taskQueue[left]);
            i = left;

        } else if(right < taskQueueSize) {
            // right children exists and is > left

            // current >= right -> current >= left, no point in swapping, return
            if(taskQueue[i]->priority >= taskQueue[right]->priority) {
                return result;
            }

            // otherwise make the swap
            h_swap(&taskQueue[i], &taskQueue[right]);
            i = right;

        } else {
            // both indices are outside of range, ie. no children
            return result;
        }
    }
}

static void h_insert(task_t *task) {
    taskQueue[taskQueueSize] = task;

    int i = taskQueueSize++;
    for(;;) {
        // is parent, nothing greater!
        if(i == 0) {
            return;
        }

        int parent = h_parent(i);

        if(taskQueue[i]->priority > taskQueue[parent]->priority) {
            // current has higher priority than parent, so swap
            h_swap(&taskQueue[i], &taskQueue[parent]);
            i = parent;
        } else {
            // parent has higher priority, return
            return;
        }
    }
}


int main() {
    task_t tasks[5];

    int i;
    for(i = 0; i < 5; ++i) {
        tasks[i].tid = i;
        tasks[i].priority = i;
        taskQueue[i] = &tasks[i];
    }
}
