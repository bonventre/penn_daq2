#include "XL3PacketTypes.h"
#include "Globals.h"
#include "DB.h"

#include "XL3Model.h"
#include "BoardID.h"

int BoardID(int crateNum, uint32_t slotMask, int updateLocation)
{
  lprintf("*** Starting Board ID ******************\n");
  XL3Packet packet;
  memset(&packet, 0, sizeof(XL3Packet));
  BoardIDReadArgs *args = (BoardIDReadArgs *) packet.payload;
  BoardIDReadResults *results = (BoardIDReadResults *) packet.payload;
  uint16_t ids[16*6];
  int crates[16*6];
  int slots[16*6];
  int positions[16*6];
  int boardcount = 0;
  
  try{

    lprintf("SLOT ID: MB     DB1     DB2     DB3     DB4     HVC\n");

    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        lprintf("%d      ",i);
        for (int j=1;j<7;j++){
          packet.header.packetType = BOARD_ID_READ_ID;
          args->slot = i;
          args->chip = j;
          args->reg = 15;
          SwapLongBlock(packet.payload,sizeof(BoardIDReadArgs)/sizeof(uint32_t));
          xl3s[crateNum]->SendCommand(&packet);
          SwapLongBlock(packet.payload,sizeof(BoardIDReadResults)/sizeof(uint32_t));
          lprintf("0x%04x ",results->id);
          int pass = 1;
          if (j==1){ //fec
            if (((results->id & 0xF000) != 0xF000) || results->id == 0xFFFF)
              pass = 0;
          }else if (j<6){//db
            if ( (results->id & 0xF000) != 0xD000)
              pass = 0;
          }else{//pmtic
            if ((results->id & 0xF000) != 0xE000)
              pass = 0;
          }
          if (pass && updateLocation){
            ids[boardcount] = results->id;
            crates[boardcount] = crateNum;
            slots[boardcount] = i;
            positions[boardcount] = j-1;
            boardcount++;
          }
        }
        lprintf("\n");
      }
    }

    if (updateLocation){
      lprintf("Updating location...\n");
      UpdateLocation(ids,crates,slots,positions,boardcount);
    }

  }
  catch(const char* s){
    lprintf("BoardID: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}


