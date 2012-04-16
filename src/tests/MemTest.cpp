#include "XL3PacketTypes.h"
#include "Globals.h"

#include "XL3Model.h"
#include "MemTest.h"

int MemTest(int crateNum, int slotNum)
{
  printf("*** Starting Mem Test ******************\n");
  XL3Packet packet;
  packet.header.packetType = MEM_TEST_ID;
  *(uint32_t *) packet.payload = slotNum;
  SwapLongBlock(packet.payload,1);
  try{
    xl3s[crateNum]->SendCommand(&packet,1,30);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  printf("****************************************\n");
  return 0;
}


