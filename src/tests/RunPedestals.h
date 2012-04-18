#ifndef _RUN_PEDESTALS_H
#define _RUN_PEDESTALS_H

#include <unistd.h>


int RunPedestals(int crateMask, uint32_t *slotMasks, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int setupCrates, int setupMTC);
int RunPedestalsEnd(int crateMask, int setupCrates, int setupMTC);

#endif

