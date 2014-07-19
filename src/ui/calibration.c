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

static volatile int snapshots[6];
static volatile unsigned int last[6];


void initCalibration() {
    unsigned int i;
    for (i = 0; i < 6; ++i) {
        snapshots[i] = 0;
        last[i] = 0;
    }
}

void printTrainSnapshot(CalibrationSnapshot_t *snapshot) {
    int offset, index;

    offset = getTermOffset() + 2;
    for (index = 0; index < 6; ++index) {
        if (snapshots[index] == 0 || snapshots[index] == snapshot->tr) {
            break;
        }
    }

    if (index < 6) {
        int snapshotHash = hash_djb2n((char*) snapshot, sizeof(CalibrationSnapshot_t));

        if (snapshotHash == last[index]) {
            return;
        }

        last[index] = snapshotHash;

        /* print the snapshot since we have space on this line */
        offset += index;
        snapshots[index] = snapshot->tr;
        printf(SAVE_CURSOR MOVE_CURSOR "| %u " "\033[%dG" "| %u      " "\033[%dG" "| %s      " "\033[%dG"
               "| %u    " "\033[%dG" "| %s     \033[%dG" "| %u      " "\033[%dG" "| %u      " "\033[%dG" "| %s-%s    "
               RESTORE_CURSOR,
               offset, RIGHT_HALF + 5, snapshot->tr, RIGHT_HALF + 13, snapshot->sp,
               RIGHT_HALF + 24, snapshot->landmark, RIGHT_HALF + 35, snapshot->dist, RIGHT_HALF + 51, snapshot->nextmark,
               RIGHT_HALF + 67, snapshot->eta, RIGHT_HALF + 81, snapshot->ata, RIGHT_HALF + 95, snapshot->headResv, snapshot->tailResv);
    }
}
