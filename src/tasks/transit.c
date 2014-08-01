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
#include <logger.h>
#include <string.h>

#define TRAIN_MAX_NUM         60
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
    TRAIN_STATION_DATA,
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
    int onBoard;
    Person passengers;
} TrainPassengers;

struct Person_t {
    int tid;
    int destination;
    int weight;
    Person next;
};


int SpawnStations(int num_stations) {
    /* spawns random stations */
    int status, attempts;

    status = 0;
    attempts = 0;
    seed(random());
    while (num_stations > 0 && attempts < 5) {
        status = AddTrainStation(random() % SENSOR_COUNT, -1);
        switch (status) {
            case TRANSACTION_INCOMPLETE:
            case TASK_DOES_NOT_EXIST:
            case TOO_MANY_STATIONS:
                attempts++;
                return status;
            case TRAIN_STATION_INVALID:
                attempts++;
                break;
            default:
                attempts = 0;
                num_stations--;
        }
    }
    return status;
}


int AddTrainStation(int sensor, int passengers) {
    TransitMessage_t msg = {.type = TRAIN_STATION_SPAWN, .arg0 = sensor};
    int status, errno;

    ASSERT(transit_system_tid != -1, "Transit System does not exist");
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


int GetTransitData(char *buffer) {
    TransitMessage_t msg = {.type = TRAIN_STATION_DATA, .arg0 = (int)buffer};
    int errno, status;

    ASSERT(transit_system_tid != -1, "Transit System does not exist");
    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("Broadcast: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
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

    ASSERT(transit_system_tid != -1, "Transit System does not exist");
    errno = Send(transit_system_tid, &msg, sizeof(msg), &nextStation, sizeof(nextStation));
    if (errno < 0) {
        error("Broadcast: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return nextStation;
}


static inline bool isValidTrainStation(TrainStation_t *stations, int sensor) {
    return (stations[sensor].active == false);
}


static int boardTrain(TrainPassengers *train, TrainStation_t *station) {
    Person tmp, passengers;
    int boarded;

    passengers = station->waiting;
    station->waiting = NULL;
    station->passengers = 0;
    boarded = 0;
    while ((tmp = passengers) != NULL) {
        /* since the pedestrian is boarding the train, they are now a passenger */
        passengers = passengers->next;
        tmp->next = train->passengers;
        tmp->tid = Create(3, Passenger);
        train->passengers = tmp;
        boarded++;
    }
    train->onBoard += boarded;
    return boarded;
}


static TrainStation_t *getRandomTrainStation(TrainStation_t *stations, int num_stations) {
    /* randomly grabs a train station from the set of train stations */
    int count, i;
    TrainStation_t *nextStation;
    TrainStation_t *available[num_stations];

    nextStation = NULL;
    count = 0;
    for (i = 0; i < SENSOR_COUNT; ++i) {
        /* figure out which sensors are available to go to */
        if (count == num_stations) {
            break;
        } else if (stations[i].active == true) {
            available[count++] = &stations[i];
        }
    }

    if (count > 0) {
        nextStation = available[random() % count];
    }
    return nextStation;
}


static void checkNonBusyTrain(TrainPassengers *trains, TrainStation_t *station) {
    int i;
    for (i = 0; i < TRAIN_MAX_NUM; ++i) {
        if (trains[i].tr > 0 && trains[i].destination < 0) {
            boardTrain(&trains[i], station);
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
    int pathWeights[SENSOR_COUNT] = {0};

    heaviestPath = -1;
    boardTrain(train, currentStation);

    tmp = train->passengers;
    while (tmp != NULL) {
        ASSERT(tmp->weight > 0, "Tried to board person with %d weight", tmp->weight);
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
    TrainPassengers train_reservations[TRAIN_MAX_NUM] = {{0}};
    Person tmp, passengers = NULL;

    initTransitIntercom();
    is_shutdown = false;
    transit_system_tid = MyTid();
    num_of_stations = 0;
    num_of_pedestrians = MAX_NUM_PEDESTRIANS;
    notice("MrBonesWildRide: Tid %d", transit_system_tid);

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
        train_stations[i].sensor = -1;
        train_stations[i].passengers = 0;
        train_stations[i].sensor = i;
    }

    for (i = 0; i < TRAIN_MAX_NUM; ++i) {
        train_reservations[i].tr = -1;
        train_reservations[i].passengers = NULL;
        train_reservations[i].destination = -1;
        train_reservations[i].onBoard = 0;
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
                if (request.arg1 >= SENSOR_COUNT || request.arg1 < 0) {
                    response = INVALID_SENSOR_ID;
                    error("MrBonesWildRide: Train %d arrived at unknown sensor %d", request.arg0, request.arg1);
                    break;
                } else {
                    TrainStation_t *station;
                    TrainPassengers *train;
                    int hash, trainId;
                    int nextStation;
                    trainId = request.arg0;
                    hash = trainId % TRAIN_MAX_NUM;
                    train = &train_reservations[hash];
                    station = &train_stations[request.arg1];
                    if (train->tr > 0 && train->tr != trainId) {
                        error("MrBonesWildRide: Collision adding train %d to spot filled by train %d", train, train->tr);
                        response = TRAIN_STATION_INVALID;
                        break;
                    }
                    d(train).tr = (train->tr <= 0 ? trainId : train->tr);
                    Log("Train %d at station %d, station is %s", train->tr, station->sensor,
                        (station->active ? "active" : "inactive"));
                    if (d(station).active == true) {
                        /* check which passengers are getting off at this stop */
                        Person stillWaiting = NULL;
                        int off;
                        off = 0;
                        Log("Train %d: Arrived at station %d with %d passengers at station",
                            train->tr, station->sensor, station->passengers);
                        while ((tmp = train->passengers) != NULL) {
                            train->passengers = train->passengers->next;
                            if (tmp->destination == station->sensor) {
                                /* this person has successfully gotten off Mr. Bone's Wild Ride */
                                Log("Train %d: Passenger got off at station %d", train->tr, station->sensor);
                                off++;
                                Destroy(tmp->tid);
                                tmp->tid = -1;
                                tmp->destination = -1;
                                tmp->weight = -1;
                                tmp->next = passengers;
                                passengers = tmp;
                                train->onBoard--;
                            } else {
                                /* the ride never ends */
                                Log("Train %d: At station %d, passenger wants to go to station %d for this passenger, "
                                    "the ride goes on", train->tr, station->sensor, tmp->destination);
                                tmp->next = stillWaiting;
                                tmp->weight++;
                                stillWaiting = tmp;
                            }
                        }
                        train->passengers = stillWaiting;
                        Log("Train %d arrived at %d, %d passengers got off", train->tr, station->sensor, off);
                        debug("MrBonesWildRide: Train %d arrived at %d, %d passengers got off", train->tr, station->sensor, off);
                    }
                    /* find the next optimal station to go to */
                    nextStation = findOptimalNextStation(station, train, train_stations);
                    Log("Train %d next station is %d (%d on board)", train->tr, nextStation, train->onBoard);
                    debug("Train %d next station is %d (%d on board)", train->tr, nextStation, train->onBoard);
                    if (nextStation >= 0) {
                        train->destination = nextStation;
                        response = 1;
                        Reply(callee, &response, sizeof(response));
                        DispatchRoute(train->tr, nextStation, 0);
                        debug("MrBonesWildRide: Train %d heading to %d now", train->tr, nextStation);
                        continue;
                    } else {
                        /* no stations need servicing now */
                        train->destination = -1;
                        Log("No stations need servicing");
                    }
                    response = 1;
                }
                break;
            case TRAIN_STATION_SPAWN:
                if (request.arg0 >= SENSOR_COUNT || request.arg0 < 0) {
                    response = INVALID_SENSOR_ID;
                } else {
                    TrainStation_t *station;
                    station = &train_stations[request.arg0];
                    if (num_of_stations >= MAX_NUM_STATIONS || num_of_stations >= SENSOR_COUNT) {
                        /* either we have added too many trains or there is already
                           a station at the given sensor */
                        response = TOO_MANY_STATIONS;
                    } else {
                        /* check if the station is valid, and add it if possible */
                        if (isValidTrainStation(train_stations, request.arg0) == true) {
                            int p_waiting;
                            station->active = true;
                            p_waiting = 0;
                            /* create the waiting passengers, each pedestrian is their own
                               task that has a certain amount of time until they die unless
                               they reach their destination first */
                            if (num_of_stations > 0) {
                                p_waiting = MIN(num_of_pedestrians, request.arg1);
                                num_of_pedestrians -= p_waiting;
                                station->passengers += p_waiting;
                                while (p_waiting --> 0) {
                                    /* generate random stations for the passengers to wait on */
                                    TrainStation_t *dest;
                                    dest = getRandomTrainStation(train_stations, num_of_stations);
                                    if (dest == NULL) {
                                        num_of_pedestrians += p_waiting;
                                        station->passengers -= p_waiting;
                                        break;
                                    }
                                    tmp = passengers;
                                    passengers = passengers->next;
                                    tmp->destination = dest->sensor;
                                    tmp->weight = 1;
                                    tmp->next = station->waiting;
                                    station->waiting = tmp;
                                }
                            }
                            tmp = NULL;
                            num_of_stations++;
                            response = 1;
                            /* need atleast two stations in order to route to stations */
                            if (num_of_stations > 0) {
                                /* now check if there's any unbusy trains and make them go there */
                                Reply(callee, &response, sizeof(response));
                                checkNonBusyTrain(train_reservations, station);
                                continue;
                            }
                            debug("MrBonesWildRide: Created station at sensor %d with %d passengers",
                                  station->sensor, station->passengers);
                        } else {
                            response = TRAIN_STATION_INVALID;
                        }
                    }
                }
                break;
            case TRAIN_STATION_REMOVE:
                if (num_of_stations > 0) {
                    if (request.arg0 >= SENSOR_COUNT || request.arg0 < 0) {
                        response = TRAIN_STATION_INVALID;
                    } else {
                        TrainStation_t *station;
                        station = &train_stations[request.arg0];
                        if (station->active == false) {
                            response = TRAIN_STATION_INVALID;
                            break;
                        }
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
                        debug("MrBonesWildRide: Removed station at %d", station->sensor);
                        station->passengers = 0;
                        station->active = false;
                    }
                } else {
                    response = NO_TRAIN_STATIONS;
                }
                break;
            case TRAIN_STATION_ADD_PASSENGERS:
                if (request.arg0 >= SENSOR_COUNT || request.arg0 < 0) {
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
                            dest = getRandomTrainStation(train_stations, num_of_stations);
                            if (dest == NULL) {
                                num_of_pedestrians += p_waiting;
                                station->passengers -= p_waiting;
                                break;
                            }
                            passengers->destination = dest->sensor;
                            passengers->weight = 1;
                            tmp = passengers;
                            passengers = passengers->next;
                            tmp->next = station->waiting;
                            station->waiting = tmp;
                        }
                        tmp = NULL;
                        response = 1;
                        Reply(callee, &response, sizeof(response));
                        checkNonBusyTrain(train_reservations, station);
                        debug("MrBonesWildRide: Station at %d has %d passengers", station->sensor, station->passengers);
                        continue;
                    }
                }
                break;
            case TRAIN_STATION_DATA:
                if (request.arg0 == 0) {
                    response = BUFFER_SPACE_INSUFF;
                } else {
                    string buffer;
                    unsigned int len;
                    char format_buffer[128];

                    buffer = (string)request.arg0;
                    response = 0;
                    len = 0;
                    for (i = 0; i < 127; ++i) {
                        format_buffer[i] = 0;
                    }

                    /* copy the station data to print */
                    for (i = 0; i < SENSOR_COUNT; ++i) {
                        if (len >= PRINT_BUFFER_SIZE) {
                            break;
                        } else  if (train_stations[i].active == true) {
                            formatas("\033[0K\033[32mSTATION %d\033[0m: %d passengers waiting\r\n", format_buffer,
                                     train_stations[i].sensor, train_stations[i].passengers);
                            strcat(buffer, format_buffer);
                            response++;
                            len += 128;
                        }
                    }

                    /* copy the train data to print */
                    for (i = 0; i < TRAIN_MAX_NUM; ++i) {
                        if (len >= PRINT_BUFFER_SIZE) {
                            break;
                        } else if (train_reservations[i].tr > 0) {
                            formatas("\033[0K\033[36mTRAIN %d\033[0m: %d passengers on board, heading to %d\r\n", format_buffer,
                                     train_reservations[i].tr, train_reservations[i].onBoard, train_reservations[i].destination);
                            strcat(buffer, format_buffer);
                            response++;
                            len += 128;
                        }
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
