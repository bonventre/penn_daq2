#ifndef _CRATE_C_BAL_H
#define _CRATE_C_BAL_H

#include <unistd.h>
#include "XL3Model.h"

#define PED_TEST_TAKEN 0x1
#define PED_CH_HAS_PEDESTALS 0x2
#define PED_RMS_TEST_PASSED 0x4
#define PED_PED_WITHIN_RANGE 0x8
#define PED_DATA_NO_ENABLE 0x10
#define PED_TOO_FEW_PER_CELL 0x20

typedef struct {
    unsigned short CrateID;
    unsigned short BoardID;
    unsigned short ChannelID;
    unsigned short CMOSCellID;
    unsigned long  GlobalTriggerID;
    unsigned short GlobalTriggerID2;
    unsigned short ADC_Qlx;
    unsigned short ADC_Qhs;
    unsigned short ADC_Qhl;
    unsigned short ADC_TAC;
    unsigned short CGT_ES16;
    unsigned short CGT_ES24;
    unsigned short Missed_Count;
    unsigned short NC_CC;
    unsigned short LGI_Select;
    unsigned short CMOS_ES16;
} ParsedBundle;

int CrateCBal(int crateNum, uint32_t slotMask, uint32_t channelMask, int updateDB, int finalTest=0, int ecal=0);

int GetPedestal(int crateNum, int slotNum, uint32_t channelMask, struct pedestal *pedestals, struct channel_params *chan_params, uint32_t *pmt_buf);
ParsedBundle ParseBundle(uint32_t *buffer);

#endif

