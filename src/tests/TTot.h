#ifndef _TTOT_H
#define _TTOT_H

#include <unistd.h>

#define MAX_TIME 1100
#define NUM_PEDS 500
#define TUB_DELAY 60

#define RMP_DEFAULT 120
#define RMPUP_DEFAULT 115
#define VSI_DEFAULT 120
#define VLI_DEFAULT 120
#define MAX_RMP_VALUE 180
#define MIN_VSI_VALUE 50

int GetTTot(int crateNum, uint32_t slotMask, int targetTime, int updateDB, int finalTest=0, int ecal=0);
int SetTTot(int crateNum, uint32_t slotMask, int targetTime, int updateDB, int finalTest=0, int ecal=0);

int MeasureTTot(int crate, uint32_t slot_mask, int start_time, uint16_t *disc_times);
int CheckTTot(int crate, int slot_num, uint32_t chan_mask, int goal_time, int *diff);
  
#endif

