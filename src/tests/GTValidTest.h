#ifndef _GT_VALID_TEST_H
#define _GT_VALID_TEST_H

#include <unistd.h>

#define VMAX 203
#define TACREF 72
#define ISETM_START 147
//#define ISETM 138
//#define ISETM 110
#define ISETM 110
#define ISETM_LONG 100
#define ISETA 70
#define ISETA_NO_TWIDDLE 0

#define GTMAX 1000
#define GTPED_DELAY 20
#define TDELAY_EXTRA 0
#define NGTVALID 20



int GTValidTest(int crateNum, uint32_t slotMask, uint32_t channelMask, float gtCutoff, int twiddleOn, int updateDB, int finalTest=0, int ecal=0);
int GetGTDelay(int crateNum, int slotNum, int wt, float *get_gtchan, uint16_t isetm0, uint16_t isetm1);

#endif

