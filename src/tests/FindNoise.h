#ifndef _FIND_NOISE_H
#define _FIND_NOISE_H

#include <unistd.h>

#define STARTING_THRESH -5
#define MAX_THRESH 30
#define SLEEP_TIME 1000000

int FindNoise(uint32_t crateMask, uint32_t *slotMasks, float frequency, int useDebug, int updateDB, int ecal=0);

#endif

