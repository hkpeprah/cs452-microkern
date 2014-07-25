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

#define SENSOR_COUNT   TRAIN_SENSOR_COUNT * TRAIN_MODULE_COUNT

void TrainDemo() {
    char buffer[50];
    int train, status, i;
    int sensors[SENSOR_COUNT] = {0};
    int train_numbers[TRAIN_COUNT] = {0};

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
                if (DispatchAddTrain(train) >= 0) {
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
        puts("Press q to quit, or any other key to continue:");
        printf("%c\r\n", ((ch = getchar()) == 'q' ? 'q' : ' '));
        if (ch == 'q') {
            break;
        }
        for (i = 0; i < train_count; ++i) {
            do {
                sensor = random() % (SENSOR_COUNT);
            } while (sensors[sensor] == 1);
            sensors[sensor] = 1;
            distance = random() % 40;
            train = train_numbers[i];
            printf("Dispatching train %u to %u mm past sensor %c%u\r\n", train, distance,
                   sensor / TRAIN_SENSOR_COUNT + 'A', sensor % TRAIN_SENSOR_COUNT + 1);
            DispatchRoute(train, sensor, distance);
        }
        for (i = 0; i < SENSOR_COUNT; ++i) {
            sensors[i] = 0;
        }
    }

    printf("Demo Complete.\r\n");
    Exit();
}
