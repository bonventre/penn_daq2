#ifndef _CHINJ_SCAN_H
#define _CHINJ_SCAN_H

#include <unistd.h>

int ChinjScan(int crateNum, uint32_t slotMask, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int numPedestals, float upper, float lower, float pmt, int pedOn, int quickOn, int updateDB, int finalTest=0);

#endif

