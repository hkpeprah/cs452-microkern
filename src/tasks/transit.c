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
#include <persona.h>

#define MAX_NUM_STATIONS      20
#define MAX_NUM_PEDESTRIANS   30
#define SENSOR_COUNT          TRAIN_MODULE_COUNT * TRAIN_SENSOR_COUNT

static volatile int transit_system_tid = -1;
struct Person_t;
typedef struct Person_t *Person;

typedef enum {
    TRAIN_STATION_ARRIVAL = 666,
    TRAIN_STATION_SPAWN,
    TRAIN_STATION_REMOVE,
    TRAIN_STATION_ADD_PASSENGERS,
    TRAIN_STATION_SHUTDOWN,
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
    int passengers;
    Person waiting;
} TrainStation_t;

typedef struct {
    int tr;
    int destination;
    Person passengers;
} TrainPassengers;

struct Person_t {
    int tid;
    int destination;
    int weight;
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


int AddPassengers(int sensor, int passengers) {
    TransitMessage_t msg = {.type = TRAIN_STATION_ADD_PASSENGERS, .arg0 = sensor, .arg1 = passengers};
    int status, errno;

    if (transit_system_tid == -1) {
        return TASK_DOES_NOT_EXIST;
    }

    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("AddPassengers: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
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


static int boardTrain(TrainPassengers *train, Person passengers) {
    Person tmp;
    int boarded;

    boarded = 0;
    while (passengers != NULL) {
        tmp = passengers;
        passengers = passengers->next;
        tmp->next = train->passengers;
        /* since the pedestrian is boarding the train, they are now a passenger */
        tmp->tid = Create(3, Passenger);
        train->passengers = tmp;
        boarded++;
    }
    return boarded;
}


static void checkNonBusyTrain(TrainPassengers *trains, TrainStation_t *station) {
    int i;
    for (i = 0; i < TRAIN_COUNT * 2; ++i) {
        if (trains[i].tr > 0 && trains[i].destination == -1) {
            boardTrain(&trains[i], station->waiting);
            station->waiting = NULL;
            trains[i].destination = station->sensor;
            DispatchRoute(trains[i].tr, station->sensor, 0);
            break;
        }
    }
}


static int findOptimalNextStation(TrainStation_t *currentStation, TrainPassengers *train, TrainStation_t *trainStations) {
    /* Finds the optimal next station by taking the union of the passengers at the
       current station and the passengers on the train, then finding the heaviest path
       which is done by:
           - finding the weights of each station based on the passengers going there
           - finding the heaviest passenger and using them if they are larger
       Each passenger has a weight that is used. */
    Person tmp;
    int heaviestPath;
    unsigned int pathWeights[SENSOR_COUNT] = {0};

    heaviestPath = -1;
    boardTrain(train, currentStation->waiting);
    currentStation->waiting = NULL;

    tmp = train->passengers;
    while (tmp != NULL) {
        pathWeights[tmp->destination] += tmp->weight;
        if (heaviestPath == -1 || pathWeights[tmp->destination] > pathWeights[heaviestPath]) {
            heaviestPath = tmp->destination;
        }
        tmp = tmp->next;
    }

    if (heaviestPath == -1) {
        /* simple linear search that just chooses the first station available */
        int i, weight;
        weight = 0;
        for (i = 0; i < SENSOR_COUNT; ++i) {
            if (trainStations[i].active != false && trainStations[i].waiting != NULL &&
                trainStations[i].passengers > weight) {
                weight = trainStations[i].passengers;
                heaviestPath = trainStations[i].sensor;
            }
        }
    }

    return heaviestPath;
}


void MrBonesWildRide() {
    bool is_shutdown;
    TransitMessage_t request;
    int num_of_stations, i;
    int callee, bytes, response, num_of_pedestrians;
    TrainStation_t train_stations[SENSOR_COUNT] = {{0}};
    struct Person_t pedestrians[MAX_NUM_PEDESTRIANS] = {{0}};
    TrainPassengers train_reservations[TRAIN_COUNT * 2] = {{0}};
    Person tmp, passengers = NULL;

    is_shutdown = false;
    transit_system_tid = MyTid();
    num_of_stations = 0;
    num_of_pedestrians = MAX_NUM_PEDESTRIANS;
    for (i = 0; i < MAX_NUM_PEDESTRIANS; ++i) {
        /* genereate the queue of people to board the train */
        pedestrians[i].next = passengers;
        pedestrians[i].tid = -1;
        pedestrians[i].destination = -1;
        pedestrians[i].weight = 0;
        passengers = &pedestrians[i];
    }

    for (i = 0; i < SENSOR_COUNT; ++i) {
        train_stations[i].active = false;
        train_stations[i].waiting = NULL;
        train_stations[i].sensor = 0;
        train_stations[i].passengers = 0;
    }

    for (i = 0; i < TRAIN_COUNT * 2; ++i) {
        train_reservations[i].tr = -1;
        train_reservations[i].passengers = NULL;
        train_reservations[i].destination = -1;
    }

    while (is_shutdown == false) {
        bytes = Receive(&callee, &request, sizeof(request));
        if (bytes < 0) {
            error("MrBonesWildRide: Error: Received %d from %d", bytes, callee);
            continue;
        }

        switch (request.type) {
            case TRAIN_STATION_SHUTDOWN:
                for (i = 0; i < MAX_NUM_PEDESTRIANS; ++i) {
                    if (pedestrians[i].tid != -1) {
                        Destroy(pedestrians[i].tid);
                    }
                }
                is_shutdown = true;
                break;
            case TRAIN_STATION_ARRIVAL:
                /* need to check for next optimal station by looking
                   for the greatest cross between next station and the passengers
                   currently on the train */
                if (request.arg1 >= SENSOR_COUNT) {
                    response = INVALID_SENSOR_ID;
                    error("MrBonesWildRide: Train %d arrived at unknown sensor %d", request.arg0, request.arg1);
                } else {
                    TrainStation_t *station;
                    TrainPassengers *train;
                    int hash, trainId;
                    trainId = request.arg0;
                    hash = hash_shift(trainId) % (TRAIN_COUNT * 2);
                    train = &train_reservations[hash];
                    station = &train_stations[request.arg1];
                    if (train->tr > 0 && train->tr != trainId) {
                        error("MrBonesWildRide: Collision adding train %d to spot filled by train %d",
                              train, train->tr);
                        response = TRAIN_STATION_INVALID;
                    } else {
                        /* check which passengers are getting off at this stop */
                        Person stillWaiting = NULL;
                        int nextStation;
                        while ((tmp = train->passengers) != NULL) {
                            train->passengers = train->passengers->next;
                            if (tmp->destination == station->sensor) {
                                /* this person has successfully gotten off Mr. Bone's Wild Ride */
                                Destroy(tmp->tid);
                                tmp->tid = -1;
                                tmp->destination = -1;
                                tmp->next = passengers;
                                tmp->weight = -1;
                                passengers = tmp;
                            } else {
                                /* the ride never ends */
                                tmp->next = stillWaiting;
                                tmp->weight++;
                                stillWaiting = tmp;
                            }
                        }
                        train->passengers = stillWaiting;
                        /* find the next optimal station to go to */
                        nextStation = findOptimalNextStation(station, train, train_stations);
                        if (nextStation >= 0) {
                            train->destination = nextStation;
                            DispatchRoute(train->tr, nextStation, 0);
                        } else {
                            /* no stations need servicing now */
                            train->destination = -1;
                        }
                        response = 1;
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
                                station->passengers += p_waiting;
                                while (p_waiting --> 0) {
                                    /* generate random stations for the passengers to wait on */
                                    TrainStation_t *dest;
                                    dest = &train_stations[random_range(0, num_of_stations)];
                                    passengers->destination = dest->sensor;
                                    passengers->weight = 1;
                                    tmp = passengers;
                                    passengers = passengers->next;
                                    tmp->next = station->waiting;
                                    station->waiting = tmp;
                                }
                            }
                            tmp = NULL;
                            num_of_stations++;
                            /* now check if there's any unbusy trains and make them go there */
                            checkNonBusyTrain(train_reservations, station);
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
                            tmp->weight = -1;
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
                        station->passengers += p_waiting;
                        while (p_waiting --> 0) {
                            /* generate random stations for the passengers to wait on */
                            TrainStation_t *dest;
                            dest = &train_stations[random_range(0, num_of_stations)];
                            passengers->destination = dest->sensor;
                            passengers->weight = 1;
                            tmp = passengers;
                            passengers = passengers->next;
                            tmp->next = station->waiting;
                            station->waiting = tmp;
                        }
                        tmp = NULL;
                        checkNonBusyTrain(train_reservations, station);
                        response = 1;
                    }
                }
                break;
            default:
                error("MrBonesWildRide: Error: Unknown request of type %d from %d", request.type, callee);
        }
        Reply(callee, &response, sizeof(response));
    }
    notice("MrBonesWildRide: Shutting down, I guess the ride does end.");
    Exit();
}
