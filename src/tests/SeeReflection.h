#ifndef _SEE_REFLECTION_H
#define _SEE_REFLECTION_H

#include <unistd.h>

#define MAX_ERRORS 50

int SeeReflection(int crateNum, uint32_t slotMask, uint32_t channelMask, int dacValue, float frequency, int updateDB, int finalTest=0);

#endif

