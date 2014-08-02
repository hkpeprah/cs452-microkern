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
#include <traincom.h>
#include <logger.h>
#include <string.h>

#define TRAIN_MAX_NUM         60
#define MAX_NUM_STATIONS      20
#define MAX_NUM_PEDESTRIANS   30
#define SENSOR_COUNT          TRAIN_MODULE_COUNT * TRAIN_SENSOR_COUNT
#define STATION_PRINT_MSG     "Station %s: %d passengers waiting at platform"
#define TRAIN_PRINT_MSG       "Train %d: heading to station at sensor %s (%d passengers on board)"

static volatile int transit_system_tid = -1;
static volatile int spawning_pool_tid = -1;
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
    char name[6];
    bool active;
    int sensor;
    int passengers;
    int serviced;
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
    int sensor;

    status = 0;
    attempts = 0;
    seed(Time());
    while (num_stations > 0 && attempts < 5) {
        sensor = random_range(0, SENSOR_COUNT - 1);
        status = AddTrainStation(sensor, -1);
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
                printf("Created station at sensor %s with %d passengers\r\n",
                       (DispatchGetTrackNode(sensor))->name, status);
        }
    }
    return 1;
}


int AddTrainStation(int sensor, int passengers) {
    /* Creates a new train station at the specified sensor with the specified
       number of passengers, a non-zero amount of passengers is generated if
       there is atleast one station.
       Returns the number of passengers at the station if success */
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

    if (passengers < 0) {
        return INVALID_PASSENGER_COUNT;
    }

    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("AddPassengers: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return status;
}


int PrintTransitData() {
    TransitMessage_t msg = {.type = TRAIN_STATION_DATA};
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

    ASSERT(transit_system_tid != -1, "Transit system does not exist");
    errno = Send(transit_system_tid, &msg, sizeof(msg), &nextStation, sizeof(nextStation));
    if (errno < 0) {
        error("Broadcast: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return nextStation;
}


int RemoveStation(int sensor) {
    TransitMessage_t msg = {.type = TRAIN_STATION_REMOVE, .arg0 = sensor};
    int errno, status;

    ASSERT(transit_system_tid != -1, "Transit system does not eixst");
    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("RemoveStation: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return status;
}


int TransitShutdown() {
    TransitMessage_t msg = {.type = TRAIN_STATION_SHUTDOWN};
    int errno, status;

    ASSERT(transit_system_tid != -1, "Transit system does not exist");
    errno = Send(transit_system_tid, &msg, sizeof(msg), &status, sizeof(status));
    if (errno < 0) {
        error("TransitShutdown: Error in Send: %d got %d sending to %d", MyTid(), errno, transit_system_tid);
        return TRANSACTION_INCOMPLETE;
    }

    return status;
}


static void SpawningPool() {
    int status, sensor, attempts, passengers, delayTime;

    delayTime = 350;
    attempts = 0;
    while (true) {
        sensor = random_range(0, SENSOR_COUNT - 1);
        if (attempts > 5) {
            debug("SpawningPool: Adding train station at %d", sensor);
            status = AddTrainStation(sensor, -1);
            attempts = 0;
        } else {
            passengers = random_range(1, 3);
            debug("SpawningPool: Adding %d passengers to station at %d", passengers, sensor);
            status = AddPassengers(sensor, passengers);
        }
        switch (status) {
            case TRANSACTION_INCOMPLETE:
                delayTime += 50;
                break;
            case TOO_MANY_STATIONS:
            case OUT_OF_PEDESTRIANS:
                delayTime *= 2;
                break;
            case TRAIN_STATION_INVALID:
            case INVALID_PASSENGER_COUNT:
                attempts++;
                delayTime = MAX(300, delayTime - 50);
                break;
            default:
                break;
        }
        Delay(delayTime);
    }
}


int AddPassengerPool() {
    /* Adds a spawning pool that adds passengers and stations
       continously over time. */
    if (spawning_pool_tid != -1) {
        /* when called with an existing spawning pool, remove it */
        Destroy(spawning_pool_tid);
        spawning_pool_tid = -1;
        return 0;
    }
    /* when called with a non-existing spawning pool, add it */
    spawning_pool_tid = Create(4, SpawningPool);
    return 1;
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
        } else if (stations[i].active == true && stations[i].serviced == -1) {
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
        if (trainStations[tmp->destination].serviced < 0) {
            pathWeights[tmp->destination] += tmp->weight;
            if (heaviestPath == -1 || pathWeights[tmp->destination] > pathWeights[heaviestPath]) {
                heaviestPath = tmp->destination;
            }
        }
        tmp = tmp->next;
    }

    if (heaviestPath == -1) {
        /* simple linear search that just chooses the first station available */
        int i, weight;
        weight = 0;
        for (i = 0; i < SENSOR_COUNT; ++i) {
            if (trainStations[i].active != false && trainStations[i].waiting != NULL &&
                trainStations[i].passengers > weight && trainStations[i].serviced < 0) {
                weight = trainStations[i].passengers;
                heaviestPath = trainStations[i].sensor;
            }
        }
    }

    if (heaviestPath >= 0) {
        trainStations[heaviestPath].serviced = train->tr;
    }
    return heaviestPath;
}


void MrBonesWildRide() {
    TransitMessage_t request;
    int num_of_stations, i;
    int callee, bytes, response, num_of_pedestrians;
    TrainStation_t train_stations[SENSOR_COUNT];
    struct Person_t pedestrians[MAX_NUM_PEDESTRIANS];
    TrainPassengers train_reservations[TRAIN_MAX_NUM];
    Person tmp, passengers = NULL;

    initTransitIntercom();
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
        track_node *node;
        node = DispatchGetTrackNode(i);
        train_stations[i].active = false;
        train_stations[i].waiting = NULL;
        train_stations[i].passengers = 0;
        train_stations[i].sensor = i;
        train_stations[i].serviced = -1;
        strcpy(train_stations[i].name, node->name);
    }

    for (i = 0; i < TRAIN_MAX_NUM; ++i) {
        train_reservations[i].tr = -1;
        train_reservations[i].passengers = NULL;
        train_reservations[i].destination = -1;
        train_reservations[i].onBoard = 0;
    }

    while (true) {
        bytes = Receive(&callee, &request, sizeof(request));
        if (bytes < 0) {
            error("MrBonesWildRide: Error: Received %d from %d", bytes, callee);
            continue;
        }

        switch (request.type) {
            case TRAIN_STATION_SHUTDOWN:
                /* wipes all the data used for the system */
                passengers = NULL;
                for (i = 0; i < MAX_NUM_PEDESTRIANS; ++i) {
                    /* clear all the pedestrians by getting rid of them */
                    pedestrians[i].next = passengers;
                    if (pedestrians[i].tid != -1) {
                        Destroy(pedestrians[i].tid);
                        pedestrians[i].tid = -1;
                    }
                    passengers = &pedestrians[i];
                }
                for (i = 0; i < SENSOR_COUNT; ++i) {
                    /* clear all stations and their passengers */
                    train_stations[i].active = false;
                    train_stations[i].waiting = NULL;
                    train_stations[i].passengers = 0;
                    train_stations[i].serviced = -1;
                }
                for (i = 0; i < TRAIN_MAX_NUM; ++i) {
                    /* clear the passengers in all the trains and wipe
                       the train data */
                    train_reservations[i].tr = -1;
                    train_reservations[i].passengers = NULL;
                    train_reservations[i].destination = -1;
                    train_reservations[i].onBoard = 0;
                }
                num_of_stations = 0;
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
                                Log("Train %d: Passenger got off at station %s", train->tr, station->name);
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
                                Log("Train %d: At station %s, passenger wants to go to station %d for this passenger, "
                                    "the ride goes on", train->tr, station->name, tmp->destination);
                                tmp->next = stillWaiting;
                                tmp->weight++;
                                stillWaiting = tmp;
                            }
                        }
                        train->passengers = stillWaiting;
                        Log("Train %d arrived at station %s, %d passengers got off", train->tr, station->name, off);
                        debug("MrBonesWildRide: Train %d arrived at station %s, %d passengers got off",
                              train->tr, station->name, off);
                    }
                    /* find the next optimal station to go to */
                    station->serviced = -1;
                    nextStation = findOptimalNextStation(station, train, train_stations);
                    Log("Train %d next station is %d (%d on board)", train->tr, nextStation, train->onBoard);
                    debug("Train %d next station is at %s (%d on board)", train->tr,
                          train_stations[nextStation].name, train->onBoard);
                    if (nextStation >= 0) {
                        train->destination = nextStation;
                        response = 1;
                        Reply(callee, &response, sizeof(response));
                        DispatchRoute(train->tr, nextStation, 0);
                        debug("MrBonesWildRide: Train %d heading to station at %s now", train->tr, train_stations[nextStation].name);
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
                                response = p_waiting;
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
                            } else {
                                response = 0;
                            }
                            tmp = NULL;
                            num_of_stations++;
                            /* need atleast two stations in order to route to stations */
                            if (num_of_stations > 0) {
                                /* now check if there's any unbusy trains and make them go there */
                                Reply(callee, &response, sizeof(response));
                                checkNonBusyTrain(train_reservations, station);
                                continue;
                            }
                            debug("MrBonesWildRide: Created station at sensor %s with %d passengers",
                                  station->name, station->passengers);
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
                        debug("MrBonesWildRide: Removed station at sensor %s", station->name);
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
                    } else if (num_of_pedestrians == 0) {
                        response = OUT_OF_PEDESTRIANS;
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
                        debug("MrBonesWildRide: Station at sensor %s has %d passengers", station->name, station->passengers);
                        continue;
                    }
                }
                break;
            case TRAIN_STATION_DATA:
                /* prints the formatted messages of where the stations/trains are to the screen */
                if (true) {
                    int num_lines_printed;
                    num_lines_printed = 0;
                    for (i = 0; i < SENSOR_COUNT; ++i) {
                        /* go through the stations and print their data */
                        if (train_stations[i].active == true) {
                            printf(STATION_PRINT_MSG "\r\n", train_stations[i].name, train_stations[i].passengers);
                            num_lines_printed++;
                        }
                    }

                    for (i = 0; i < TRAIN_MAX_NUM; ++i) {
                        /* go through the trains and print their data */
                        if (train_reservations[i].tr >= 0) {
                            int destination = train_reservations[i].destination;
                            printf(TRAIN_PRINT_MSG "\r\n", train_reservations[i].tr,
                                   (destination == -1 ? "NONE" : train_stations[destination].name), train_reservations[i].onBoard);
                            num_lines_printed++;
                        }
                    }

                    move_cur_up(num_lines_printed);
                    response = num_lines_printed;
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
