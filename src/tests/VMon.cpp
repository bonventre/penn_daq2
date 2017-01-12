#include <stdio.h>
#include "math.h"

#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "VMon.h"

#define RED "\x1b[31m"
#define RESET "\x1b[0m"

int VMon(int crateNum, uint32_t slotMask, int updateDB, int finalTest)
{
  lprintf("*** Starting VMon **********************\n");

  int count_bad_voltages = 0;
  float threshold = 0.1; // 10% threshold to warn on bad voltages
  float const_voltage[18] = {-24.0,-15.0,-5.0,-3.3,-2.0,3.3,4.0,
                             5.0,6.5,8.0,15.0,24.0,-2.0,-1.0,0.8,
                             1.0,4.0,5.0};
  float bad_voltage[18];
  float voltages[16][21];
  for (int i=0;i<16;i++){
    for (int j=0;j<21;j++){
      voltages[i][j] = 0;
      if(j < 19)
        bad_voltage[j] = fabs(const_voltage[j]*threshold);
    }
  }

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
      lprintf("slot             %2d     %2d     %2d     %2d     %2d     %2d     %2d     %2d\n",k*8,k*8+1,k*8+2,k*8+3,k*8+4,k*8+5,k*8+6,k*8+7);
      for (int i=0;i<21;i++){
        lprintf("%10s   ",voltages_name[i]);
        for (int j=0;j<8;j++){
          if(voltages[j+k*8][i] != 0.00 && i < 18 &&
             voltages[j+k*8][i] > const_voltage[i] + bad_voltage[i]){
            lprintf(RED "%6.2f " RESET,voltages[j+k*8][i]);
            count_bad_voltages++;
          }
          else if(voltages[j+k*8][i] != 0.00 && i < 18 &&
             voltages[j+k*8][i] < const_voltage[i] - bad_voltage[i]){
            lprintf(RED "%6.2f " RESET,voltages[j+k*8][i]);
            count_bad_voltages++;
          }
          else
            lprintf("%6.2f ",voltages[j+k*8][i]);
        }
        lprintf("\n");
      }
      lprintf("\n");
    }

    // update the database
    if (updateDB){
      lprintf("updating the database\n");
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
    lprintf("VMon: %s\n",s);
  }
  
  float vth = threshold*100; 
  lprintf("%d bad voltage(s) (%.1f%% of nominal) \n", count_bad_voltages,vth);
  lprintf("****************************************\n");
  return 0;
}

