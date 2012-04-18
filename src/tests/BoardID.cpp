#include "XL3PacketTypes.h"
#include "Globals.h"

#include "XL3Model.h"
#include "BoardID.h"

int BoardID(int crateNum, uint32_t slotMask)
{
  printf("*** Starting Board ID ******************\n");
  XL3Packet packet;
  BoardIDReadArgs *args = (BoardIDReadArgs *) packet.payload;
  BoardIDReadResults *results = (BoardIDReadResults *) packet.payload;
  try{

    printf("SLOT ID: MB     DB1     DB2     DB3     DB4     HVC\n");

    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        printf("%d      ",i);
        for (int j=1;j<7;j++){
          packet.header.packetType = BOARD_ID_READ_ID;
          args->slot = i;
          args->chip = j;
          args->reg = 15;
          SwapLongBlock(packet.payload,sizeof(BoardIDReadArgs)/sizeof(uint32_t));
          xl3s[crateNum]->SendCommand(&packet);
          SwapLongBlock(packet.payload,sizeof(BoardIDReadResults)/sizeof(uint32_t));
          printf("0x%04x ",results->id);
        }
        printf("\n");
      }
    }


  }
  catch(const char* s){
    printf("BoardID: %s\n",s);
  }

  printf("****************************************\n");
  return 0;
}


