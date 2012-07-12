#ifndef _MTC_CMDS_H
#define _MTC_CMDS_H

int SBCControl(int connect, int kill, int manual, const char *idFile);
int MTCInit(int xilinx);
int MTCRead(uint32_t address);
int MTCWrite(uint32_t address, uint32_t data);
  
int MTCDelay(float time);
#endif

