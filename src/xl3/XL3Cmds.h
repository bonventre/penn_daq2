#include <unistd.h>

int XL3RW(int crateNum, uint32_t address, uint32_t data);
int XL3QueueRW(int crateNum, uint32_t address, uint32_t data);
int FECTest(int crateNum, uint32_t slotMask);
int CrateInit(int crateNum,uint32_t slotMask, int xilinxLoad, int hvReset, int shiftRegOnly,
    int useVBal, int useVThr, int useTDisc, int useTCmos, int useAll, int useHw);
int SMReset(int crateNum);
int DebuggingMode(int crateNum, int on);
int ChangeMode(int crateNum, int mode, uint32_t dataAvailMask);
int ReadLocalVoltage(int crateNum, int voltage);
int HVReadback(int crateNum);
int SetAlarmDac(int crateNum, uint32_t *dacs);
int LoadRelays(int crateNum, uint32_t *patterns);

