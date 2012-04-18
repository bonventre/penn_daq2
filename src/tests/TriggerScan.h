#ifndef _TRIGGER_SCAN_H
#define _TRIGGER_SCAN_H

#include <unistd.h>

int TriggerScan(uint32_t crateMask, uint32_t *slotMasks, int triggerSelect, int dacSelect, int maxNhit, int minThresh, const char* fileName, int quickMode);

#endif

