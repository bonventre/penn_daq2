#ifndef _ZDISC_H
#define _ZDISC_H

#include <unistd.h>

int ZDisc(int crateNum, uint32_t slotMask, float rate, int offset, int updateDB, int finalTest=0, int ecal=0);

#endif

