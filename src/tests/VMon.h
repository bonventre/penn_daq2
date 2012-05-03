#ifndef _VMON_H
#define _VMON_H

#include <unistd.h>

static const char voltages_name[21][20] = {"neg_24","neg_15","Vee","neg_3_3","neg_2","pos_3_3","pos_4","Vcc","pos_6_5","pos_8","pos_15","pos_24","neg_2_ref","neg_1_ref","pos_0_8_ref","pos_1_ref","pos_4_ref","pos_5_ref","Temp","CalD","hvt"};
static const float voltages_min[21] = {-26.0,-17.0,-6.0,-4.0,-3.0,2.6,3.5,4.0,5.5,7.0,13.0,22.0,-3.0,-2.0,0.5,0.5,3.5,4.0,-99.0,-99.0,-99.0};
static const float voltages_max[21] = {-22.0,-13.0,-4.0,-2.0,-1.0,4.0,4.5,6.0,7.5,9.0,17.0,26.0,-1.0, 0.0,1.0,1.5,4.5,6.0,99.0,99.0,99.0};
static const float voltages_nom[21] = {-24.0,-15.0,-5.0,-3.3,-2.0,3.3,4.0,5.0,6.5,8.0,15.0,24.0,-2.0,-1.0,0.8,1.0,4.0,5.0,0.0,0.0,0.0};

int VMon(int crateNum, uint32_t slotMask, int updateDB, int finalTest=0);

#endif

