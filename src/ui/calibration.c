/*
 * calibration.c
 * description:
 *     i.  the train’s location in real-time in the form of a landmark, a
 *         distance (in centimetres) before or beyond it plus the direction of
 *         travel of the train,
 *     ii. the expected and actual time (or distance) of the train at the previous
 *         landmark.
 */
#include <types.h>
#include <term.h>
#include <stdlib.h>
#include <calibration.h>
#include <train_speed.h>
#include <hash.h>

#define SNAPSHOT_COUNT    6

typedef struct {
    int tr_number;
    unsigned int hash;
    CalibrationSnapshot_t snapshot;
} Snapshot_t;

static volatile Snapshot_t snapshots[SNAPSHOT_COUNT];


void initCalibration() {
    unsigned int i;
    for (i = 0; i < SNAPSHOT_COUNT; ++i) {
        snapshots[i].tr_number = 0;
        snapshots[i].hash = 0;
    }
}


void printTrainSnapshot(CalibrationSnapshot_t *snapshot) {
    int offset, index;

    offset = getTermOffset() + 2;
    for (index = 0; index < SNAPSHOT_COUNT; ++index) {
        if (snapshots[index].tr_number == 0 || snapshots[index].tr_number == snapshot->tr) {
            snapshots[index].tr_number = snapshot->tr;
            break;
        }
    }

    if (index < SNAPSHOT_COUNT) {
        int snapshotHash;
        snapshotHash = hash_djb2n((char*)snapshot, sizeof(CalibrationSnapshot_t));

        if (snapshotHash == snapshots[index].hash) {
            return;
        }

        /* print the snapshot since we have space on this line */
        snapshots[index].hash = snapshotHash;
        offset += index;
        memcpy((void*)&(snapshots[index].snapshot), snapshot, sizeof(CalibrationSnapshot_t));
        printf(SAVE_CURSOR MOVE_CURSOR "| %u " "\033[%dG" "| %u      " "\033[%dG" "| %s      " "\033[%dG"
               "| %u    " "\033[%dG" "| %s     \033[%dG" "| %u      " "\033[%dG" "| %u      " "\033[%dG" "| %s - %s    "
               "\033[%dG |" RESTORE_CURSOR,
               offset, RIGHT_HALF + 5, snapshot->tr, RIGHT_HALF + 13, snapshot->sp,
               RIGHT_HALF + 24, snapshot->landmark, RIGHT_HALF + 35, snapshot->dist, RIGHT_HALF + 51, snapshot->nextmark,
               RIGHT_HALF + 67, snapshot->eta, RIGHT_HALF + 81, snapshot->ata, RIGHT_HALF + 95, snapshot->headResv,
               snapshot->tailResv, RIGHT_HALF + 110);
    }
}


void clearTrainSnapshot(unsigned int tr) {
    int offset, index;
    int j = 0;

    offset = getTermOffset() + 2;
    for (index = 0; index < SNAPSHOT_COUNT; ++index) {
        if (snapshots[index].tr_number == tr) {
            for (j = index; j < SNAPSHOT_COUNT - 1; ++j) {
                if (snapshots[j + 1].tr_number == 0) {
                    break;
                }
                snapshots[j].hash = 0;
                snapshots[j].tr_number = snapshots[j + 1].tr_number;
                memcpy((void*)&(snapshots[j].snapshot), (void*)&(snapshots[j + 1].snapshot), sizeof(CalibrationSnapshot_t));
                printTrainSnapshot((CalibrationSnapshot_t*)&(snapshots[j].snapshot));
            }
            snapshots[j].tr_number = 0;
            snapshots[j].hash = 0;
            break;
        }
    }

    if (index < SNAPSHOT_COUNT && j < SNAPSHOT_COUNT) {
        /* erase this line as this is at the end of the snapshot */
        printf(SAVE_CURSOR MOVE_CURSOR "\033[0K" RESTORE_CURSOR, offset + j, RIGHT_HALF + 5);
    }
}
