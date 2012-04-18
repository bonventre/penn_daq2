#ifndef _VMON_H
#define _VMON_H

#include <unistd.h>

static const char voltages_name[21][20] = {"neg_24","neg_15","Vee","neg_3_3","neg_2","pos_3_3","pos_4","Vcc","pos_6_5","pos_8","pos_15","pos_24","neg_2_ref","neg_1_ref","pos_0_8_ref","pos_1_ref","pos_4_ref","pos_5_ref","Temp","CalD","hvt"};
static const float voltages_min[21] = {-26.,-17.,-6.,-4.,-3.,2.5,3.,4.,5.5,7.,13.,22.,-3.,-2.,0.2,0.5,3,4,-99.,-99.,-99};
static const float voltages_max[21] = {-22.,-13.,-4.,-2.,-1.,4.5,5.,6.,7.5,9.,17.,26.,-1.,0.,2.,2.,5.,6.,99.,99.,99};
static const float voltages_nom[21] = {-24,-15,-5,-3.3,-2,3.3,4,5,6.5,8,15,24,-2,-1,0.8,1,4,5,0,0,0};

int VMon(int crateNum, uint32_t slotMask, int updateDB, int finalTest=0);

#endif

