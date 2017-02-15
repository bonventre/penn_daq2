#ifndef _SEE_REFLECTION_ESUM_H
#define _SEE_REFLECTION_ESUM_H

#include <unistd.h>

#define MAX_ERRORS 50

int SeeReflectionEsum(int crateNum, uint32_t slotMask, int dacValue, float frequency, int updateDB, int finalTest=0);

#endif

