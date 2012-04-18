#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "VMon.h"

int VMon(int crateNum, uint32_t slotMask, int updateDB, int finalTest)
{
  printf("*** Starting VMon **********************\n");

  float voltages[16][21];
  for (int i=0;i<16;i++)
    for (int j=0;j<21;j++)
      voltages[i][j] = 0;

  try {

    XL3Packet packet;
    VMonArgs *args = (VMonArgs *) packet.payload;
    VMonResults *results = (VMonResults *) packet.payload;
    for (int slot=0;slot<16;slot++){
      if ((0x1<<slot) & slotMask){
        packet.header.packetType = VMON_ID;
        args->slotNum = slot;
        SwapLongBlock(args,sizeof(VMonArgs)/sizeof(uint32_t));
        xl3s[crateNum]->SendCommand(&packet);

        SwapLongBlock(results,sizeof(VMonResults)/sizeof(uint32_t));
        for (int i=0;i<21;i++)
          voltages[slot][i] = results->voltages[i];
      }
    }

    // now lets print out the results
    for (int k=0;k<2;k++){
      printf("slot             %2d     %2d     %2d     %2d     %2d     %2d     %2d     %2d\n",k*8,k*8+1,k*8+2,k*8+3,k*8+4,k*8+5,k*8+6,k*8+7);
      for (int i=0;i<21;i++){
        printf("%10s   ",voltages_name[i]);
        for (int j=0;j<8;j++){
          printf("%6.2f ",voltages[j+k*8][i]);
        }
        printf("\n");
      }
      printf("\n");
    }

    // update the database
    if (updateDB){
      printf("updating the database\n");
      char hextostr[50];
      for (int slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          int pass_flag = 1;
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("vmon"));

          JsonNode *all_volts = json_mkarray();
          for (int j=0;j<18;j++){
            JsonNode *one_volt = json_mkobject();
            json_append_member(one_volt,"name",json_mkstring(voltages_name[j]));
            json_append_member(one_volt,"nominal",json_mknumber((double) voltages_nom[j]));
            json_append_member(one_volt,"value",json_mknumber((double)voltages[slot][j]));
            json_append_member(one_volt,"ok",json_mkbool((voltages[slot][j] >= voltages_min[j]) && (voltages[slot][j] <= voltages_max[j])));
            json_append_element(all_volts,one_volt);
            if ((voltages[slot][j] < voltages_min[j]) || (voltages[slot][j] > voltages_max[j]))
              pass_flag = 0;
          }
          json_append_member(newdoc,"voltages",all_volts);

          json_append_member(newdoc,"temp",json_mknumber((double)voltages[slot][18]));
          json_append_member(newdoc,"cald",json_mknumber((double)voltages[slot][19]));
          json_append_member(newdoc,"hvt",json_mknumber((double)voltages[slot][20]));

          json_append_member(newdoc,"pass",json_mkbool(pass_flag));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc);
        }
      }
    }

  }
  catch(const char* s){
    printf("VMon: %s\n",s);
  }

  printf("****************************************\n");
  return 0;
}

