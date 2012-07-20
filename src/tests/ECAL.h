#ifndef _ECAL_H
#define _ECAL_H

#include <unistd.h>

int ECAL(uint32_t crateMask, uint32_t *slotMasks, uint32_t testMask, const char* loadECAL, int createFECDocs);

static const char testList[11][30] = {"fec_test","board_id","cgt_test","crate_cbal","ped_run","set_ttot","get_ttot","disc_check","gtvalid_test","zdisc","find_noise"};

#endif

