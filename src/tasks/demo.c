/*
 * demo.c - Train Control Demo 2
 */
#include <demo.h>
#include <string.h>
#include <term.h>
#include <stdio.h>
#include <dispatcher.h>
#include <train.h>
#include <train_speed.h>
#include <random.h>


void TrainDemo() {
    char buffer[50];
    int train, status, i;
    int train_numbers[TRAIN_COUNT];

    i = 0;
    while (i < TRAIN_COUNT) {
        puts("Enter train number or c to continue: ");
        gets(IO, buffer, 50);
        if (strcmp(buffer, "c") == 0) {
            break;
        } else {
            train = atoin(buffer, &status);
            if (status == 0) {
                printf("Invalid input: %s\r\n", buffer);
            } else {
                status = DispatchAddTrain(train);
                if (status == 0 || status == 1) {
                    train_numbers[i++] = train;
                }
            }
        }
    }

    if (i == 0) {
        puts("No train given for demo, exiting.\r\n");
        Exit();
    }

    int train_count = i;
    int sensor, distance;
    while (true) {
        char ch;
        puts("Press any character to run the demo: ");
        printf("%c\r\n", (ch = getchar()));
        for (i = 0; i < train_count; ++i) {
            sensor = random() % (TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT);
            distance = random() % 40;
            train = train_numbers[i];
            printf("Dispatching train %u to %u mm past sensor %c%u\r\n", train, distance,
                   sensor / TRAIN_SENSOR_COUNT + 'A', sensor % TRAIN_SENSOR_COUNT + 1);
            DispatchRoute(train, sensor, distance);
        }
        puts("Press q to quit, or c to continue: ");
        printf("%c\r\n", (ch = getchar()));
        if (ch == 'c') {
            continue;
        } else {
            break;
        }
    }

    printf("Demo Complete.\r\n");
    Exit();
}
