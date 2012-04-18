#ifndef _FIFO_TEST_H
#define _FIFO_TEST_H

#include <unistd.h>

int FifoTest(int crateNum, uint32_t slotMask, int updateDB, int finalTest=0);
static void CheckFifo(int crateNum, int slotNum, uint32_t *thediff, char *msg_buff);
void DumpPmtVerbose(int n, uint32_t *pmt_buf, char* msg_buf);

#endif

