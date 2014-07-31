/*
 * transit.c - the real-time transit system
 */
#include <track_node.h>
#include <train_task.h>
#include <dispatcher.h>
#include <transit.h>
#include <server.h>
#include <util.h>
#include <clock.h>
#include <syscall.h>
#include <types.h>
#include <train_speed.h>
#include <term.h>
#include <random.h>
#include <train.h>
#include <hash.h>

#define MAX_NUM_STATIONS      20
#define MAX_NUM_PEDESTRIANS   30
#define SENSOR_COUNT          TRAIN_MODULE_COUNT * TRAIN_SENSOR_COUNT

static int transit_system_tid = -1;
struct Person_t;
typedef struct Person_t *Person;

typedef enum {
    TRAIN_STATION_ARRIVAL = 666,
    TRAIN_STATION_SPAWN,
    TRAIN_STATION_REMOVE,
    TRAIN_STATION_ADD_PASSENGERS,
    NUM_FUHRER_MESSAGE_TYPES
} TransitMessageTypes;

typedef struct {
    TransitMessageTypes type;
    int arg0;
    int arg1;
} TransitMessage_t;

typedef struct {
    bool active;
    int sensor;
    Person waiting;
} TrainStation_t;

typedef struct {
    int tr;
    Person passengers;
} TrainPassengers;

struct Person_t {
    int tid;
    int destination;
    Person next;
};


int AddTrainStation(int sensor, int passengers) {
    TransitMessage_t msg = {.type = TRAIN_STATION_SPAWN, .arg0 = sensor};
    int status, errno;

    if (transit_system_tid == -1) {
        return TASK_DOES_NOT_EXIST;
    }

    if (passengers < 0) {
        passengers = random_range(1, 5);
    }

    msg.arg1 = passengers;
    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("AddTrainStation: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return status;
}


int Broadcast(int train, int sensor) {
    /* Broadcast the arrival of a train at a sensor (station) to the
       MrBonesWildRide which determines if there is another place the train
       should then go to */
    TransitMessage_t msg = {.type = TRAIN_STATION_ARRIVAL, .arg0 = train, .arg1 = sensor};
    int errno, nextStation;

    if (transit_system_tid == -1) {
        return TASK_DOES_NOT_EXIST;
    }

    errno = Send(transit_system_tid, &msg, sizeof(msg), &nextStation, sizeof(nextStation));
    if (errno < 0) {
        error("Broadcast: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return nextStation;
}


static inline bool isValidTrainStation(TrainStation_t *stations, int sensor) {
    int i;
    bool isValid;

    isValid = true;
    for (i = 0; i < SENSOR_COUNT; ++i) {
        isValid &= (stations[i].sensor == false ||
                    (stations[i].sensor != sensor && stations[i].sensor != sensor + 1 && stations[i].sensor != sensor - 1));
        if (isValid == false) {
            return false;
        }
    }
    return isValid;
}


static int findOptimalNextStation(TrainStation_t *stations, TrainPassengers *reservation) {
    return -1;
}


static void Passenger() {
    Exit();
}


void MrBonesWildRide() {
    TransitMessage_t request;
    int num_of_stations, i;
    int callee, bytes, response, num_of_pedestrians;
    TrainStation_t train_stations[SENSOR_COUNT] = {{0}};
    struct Person_t pedestrians[MAX_NUM_PEDESTRIANS] = {{0}};
    TrainPassengers train_reservations[TRAIN_COUNT * 2] = {{0}};
    Person tmp, passengers = NULL;

    transit_system_tid = MyTid();
    num_of_stations = 0;
    num_of_pedestrians = MAX_NUM_PEDESTRIANS;
    for (i = 0; i < MAX_NUM_PEDESTRIANS; ++i) {
        /* genereate the queue of people to board the train */
        pedestrians[i].next = passengers;
        pedestrians[i].tid = -1;
        pedestrians[i].destination = -1;
        passengers = &pedestrians[i];
    }

    for (i = 0; i < SENSOR_COUNT; ++i) {
        train_stations[i].active = false;
        train_stations[i].waiting = NULL;
        train_stations[i].sensor = 0;
    }

    for (i = 0; i < TRAIN_COUNT * 2; ++i) {
        train_reservations[i].tr = -1;
        train_reservations[i].passengers = NULL;
    }

    while (true) {
        bytes = Receive(&callee, &request, sizeof(request));
        if (bytes < 0) {
            error("MrBonesWildRide: Error: Received %d from %d", bytes, callee);
            continue;
        }

        switch (request.type) {
            case TRAIN_STATION_ARRIVAL:
                /* need to check for next optimal station by looking
                   for the greatest cross between next station and the passengers
                   currently on the train */
                if (request.arg1 >= SENSOR_COUNT) {
                    response = INVALID_SENSOR_ID;
                    error("MrBonesWildRide: Train %d arrived at unknown sensor %d", request.arg0, request.arg1);
                } else {
                    TrainStation_t *station;
                    TrainPassengers *reservation;
                    int hash, train;
                    train = request.arg0;
                    hash = hash_shift(train) % (TRAIN_COUNT * 2);
                    reservation = &train_reservations[hash];
                    station = &train_stations[request.arg1];
                    if (reservation->tr > 0 && reservation->tr != train) {
                        error("MrBonesWildRide: Collision adding train %d to spot filled by train %d",
                              train, reservation->tr);
                        response = TRAIN_STATION_INVALID;
                    } else {
                        /* check which passengers are getting off at this stop */
                        Person stillWaiting = NULL;
                        while ((tmp = reservation->passengers) != NULL) {
                            reservation->passengers = reservation->passengers->next;
                            if (tmp->destination == station->sensor) {
                                /* this person has successfully gotten off Mr. Bone's Wild Ride */
                                Destroy(tmp->tid);
                                tmp->tid = -1;
                                tmp->destination = -1;
                                tmp->next = passengers;
                                passengers = tmp;
                            } else {
                                /* the ride never ends */
                                tmp->next = stillWaiting;
                                stillWaiting = tmp;
                            }
                        }
                        reservation->passengers = stillWaiting;
                        /* find the next optimal station to go to */
                        response = findOptimalNextStation(train_stations, reservation);
                    }
                }
                break;
            case TRAIN_STATION_SPAWN:
                if (request.arg0 >= SENSOR_COUNT) {
                    response = INVALID_SENSOR_ID;
                } else {
                    TrainStation_t *station;
                    station = &train_stations[request.arg0];
                    if (num_of_stations >= MAX_NUM_STATIONS || num_of_stations >= SENSOR_COUNT || station->active) {
                        /* either we have added too many trains or there is already
                           a station at the given sensor */
                        response = TOO_MANY_STATIONS;
                    } else {
                        /* check if the station is valid, and add it if possible */
                        if (isValidTrainStation(train_stations, request.arg0) == true) {
                            int p_waiting;
                            station->active = true;
                            station->sensor = request.arg0;
                            /* need atleast two stations in order to route to stations */
                            if (num_of_stations > 0) {
                                /* create the waiting passengers, each pedestrian is their own
                                   task that has a certain amount of time until they die unless
                                   they reach their destination first */
                                p_waiting = MIN(num_of_pedestrians, request.arg1);
                                num_of_pedestrians -= p_waiting;
                                while (p_waiting --> 0) {
                                    /* generate random stations for the passengers to wait on */
                                    TrainStation_t *dest;
                                    passengers->tid = Create(3, Passenger);
                                    dest = &train_stations[random_range(0, num_of_stations)];
                                    passengers->destination = dest->sensor;
                                    tmp = passengers;
                                    passengers = passengers->next;
                                    tmp->next = station->waiting;
                                    station->waiting = tmp;
                                }
                            }
                            tmp = NULL;
                            num_of_stations++;
                            response = 1;
                        } else {
                            response = TRAIN_STATION_INVALID;
                        }
                    }
                }
                break;
            case TRAIN_STATION_REMOVE:
                if (num_of_stations > 0) {
                    if (request.arg0 >= SENSOR_COUNT) {
                        response = TRAIN_STATION_INVALID;
                    } else {
                        TrainStation_t *station;
                        station = &train_stations[request.arg0];
                        while ((tmp = station->waiting) != NULL) {
                            /* clear the passengers that were waiting on this train, I
                               guess they just die, oh well */
                            station->waiting = station->waiting->next;
                            tmp->next = passengers;
                            tmp->tid = -1;
                            passengers  = tmp;
                            num_of_pedestrians++;
                        }
                        num_of_stations--;
                        response = 1;
                    }
                } else {
                    response = NO_TRAIN_STATIONS;
                }
                break;
            case TRAIN_STATION_ADD_PASSENGERS:
                if (request.arg0 >= SENSOR_COUNT) {
                    response = INVALID_SENSOR_ID;
                } else {
                    TrainStation_t *station;
                    station = &train_stations[request.arg0];
                    if (num_of_stations == 0 || station->active == false) {
                        response = TRAIN_STATION_INVALID;
                    } else {
                        int p_waiting;
                        /* create the waiting passengers, each pedestrian is their own
                           task that has a certain amount of time until they die unless
                           they reach their destination first */
                        p_waiting = MIN(num_of_pedestrians, request.arg1);
                        num_of_pedestrians -= p_waiting;
                        while (p_waiting --> 0) {
                            /* generate random stations for the passengers to wait on */
                            TrainStation_t *dest;
                            passengers->tid = Create(3, Passenger);
                            dest = &train_stations[random_range(0, num_of_stations)];
                            passengers->destination = dest->sensor;
                            tmp = passengers;
                            passengers = passengers->next;
                            tmp->next = station->waiting;
                            station->waiting = tmp;
                        }
                        tmp = NULL;
                        response = 1;
                    }
                }
                break;
            default:
                error("MrBonesWildRide: Error: Unknown request of type %d from %d", request.type, callee);
        }
        Reply(callee, &response, sizeof(response));
    }

    Exit();
}
