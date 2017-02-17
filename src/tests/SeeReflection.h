#ifndef _SCAN_REFLECTIONS_H
#define _SCAN_REFLECTIONS_H

#include <unistd.h>

#define MAX_ERRORS 50

int ScanReflection(int crateNum, uint32_t slotMask, uint32_t channelMask, int triggerSelect, uint16_t dacCounts, float frequency, int updateDB, int finalTest=0);

#endif

