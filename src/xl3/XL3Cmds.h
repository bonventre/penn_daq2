#include <unistd.h>

int XL3RW(int crateNum, uint32_t address, uint32_t data);
int FECTest(int crateNum, uint32_t slotMask);
int CrateInit(int crateNum,uint32_t slotMask, int xilinxLoad, int hvReset, int shiftRegOnly,
    int useVBal, int useVThr, int useTDisc, int useTCmos, int useAll, int useHw);

