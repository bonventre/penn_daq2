#ifndef _CALD_TEST_H
#define _CALD_TEST_H

#include <unistd.h>

#define MAX_SAMPLES 10000

int CaldTest(int crateNum, uint32_t slotMask, int upper, int lower, int numPoints, int samples, int updateDB, int finalTest=0);

#endif

