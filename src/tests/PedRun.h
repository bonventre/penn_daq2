#ifndef _PED_RUN_H
#define _PED_RUN_H

#include <unistd.h>

#define TACBAR_MAX 2600
#define TACBAR_MIN 1800

int PedRun(int crateNum, uint32_t slotMask, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int numPedestals, int upper, int lower, int updateDB, int balanced=0, int finalTest=0, int ecal=0);

#endif

