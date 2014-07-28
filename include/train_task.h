#ifndef __TRAIN_TASK_H__
#define __TRAIN_TASK_H__
#include <track_node.h>


// shared with conductor
typedef enum {
    GOTO_COMPLETE = 28599,      // completed route
    GOTO_REROUTE,               // cannot complete route, request reroute
    GOTO_LOST,                  // totally lost, please put me out of my misery
    GOTO_NONE                   // wat
} GotoResult_t;


int TrCreate(int priority, int tr, track_node *start_sensor);
int TrDelete(unsigned int tid);
int TrSpeed(unsigned int tid, unsigned int speed);
int TrDir(unsigned int tid);
int TrReverse(unsigned int tid);
int TrDirection(unsigned int tid);
int TrAuxiliary(unsigned int tid, unsigned int aux);
int TrGetSpeed(unsigned int tid);
GotoResult_t TrGotoAfter(unsigned int tid, track_node **path, unsigned int pathLen, unsigned int dist);
track_node *TrGetLocation(unsigned int tid, unsigned int *dist);
track_edge *TrGetEdge(unsigned int tid);

#endif /* __TRAIN_TASK_H__ */
