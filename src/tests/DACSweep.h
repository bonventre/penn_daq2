#ifndef _DAC_SWEEP_H
#define _DAC_SWEEP_H

#include <unistd.h>

int DACSweep(int crateNum, uint32_t slotMask, uint32_t dacMask, int dacNum, int updateDB);
int SingleDacSweep(int crateNum, int slotNum, int dacNum);

#endif

