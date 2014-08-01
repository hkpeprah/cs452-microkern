#ifndef __STATION_H__
#define __STATION_H__

#define TOO_MANY_STATIONS          -43
#define NO_TRAIN_STATIONS          -44
#define TRAIN_STATION_INVALID      -45


void MrBonesWildRide();
int GetTransitData(char *buffer);
int Broadcast(int train, int sensor);
int AddTrainStation(int sensor, int passengers);
int AddPassenger(int sensor, int passengers);
int SpawnStations(int num_stations);

#endif /* __STATION_H__ */
