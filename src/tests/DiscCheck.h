#ifndef _DISC_CHECK_H
#define _DISC_CHECK_H

#include <unistd.h>

#define MAX_PER_PACKET 5000

int DiscCheck(int crateNum, uint32_t slotMask, int numPeds, int updateDB, int finalTest=0, int ecal=0);

#endif

