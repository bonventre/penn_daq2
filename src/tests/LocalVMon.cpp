#include "XL3PacketTypes.h"
#include "Globals.h"
#include "DB.h"

#include "XL3Model.h"
#include "BoardID.h"

int LocalVMon(int crateNum)
{
  lprintf("Starting local vmon:\n"); 
  XL3Packet packet;
  LocalVMonResults *results = (LocalVMonResults *) packet.payload;
  packet.header.packetType = LOCAL_VMON_ID;
  try{
  xl3s[crateNum]->SendCommand(&packet);
  }
  catch(const char* s){
    lprintf("LocalVMon: %s\n",s);
  }
  SwapLongBlock(packet.payload,sizeof(LocalVMonResults)/sizeof(uint32_t));
  for (int i=0;i<8;i++){
    lprintf("%d: %f\n",i,results->voltages[i]);
  }
  return 0;
}
