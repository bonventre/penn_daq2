#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "ControllerLink.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "ScanReflections.h"

int ScanReflection(int crateNum, uint32_t slotMask, uint32_t channelMask, int triggerSelect, uint16_t dacCounts, float frequency, int updateDB)
{
  lprintf("*** Starting See Reflection ************\n");
  lprintf("MAKE SURE YOU HAVE REINITIALIZED WITH TRIGGERS ENABLED FIRST! (-t OPTION IN CRATE_INIT\n");

  char channel_results[32][100];

  uint16_t counts[14];
  for(int i = 0; i < 14; i++){
    if(i == triggerSelect)
      counts[i] = dacCounts;
    else
      counts[i] = 0;
  } 

  try {

    // set up pulser
    int errors = mtc->SetupPedestals(frequency, DEFAULT_PED_WIDTH, DEFAULT_GT_DELAY,0,
        (0x1<<crateNum), MSK_TUB | MSK_TUB_B);
    if (errors){
      lprintf("Error setting up MTC for pedestals. Exiting\n");
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);
      return -1;
    }

    mtc->UnsetGTMask(0xFFFFFFFF);
    mtc->LoadMTCADacsByCounts(counts);
    usleep(500);
    mtc->SetGTMask(0x1<<(triggerSelect)-1);

    // loop over slots
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        // loop over channels
        for (int j=0;j<32;j++){
          if ((0x1<<j) & channelMask){
            uint32_t temp_pattern = 0x1<<j;
            memset(channel_results[j],'\0',100);

            // turn on pedestals for just this one channel
            errors += xl3s[crateNum]->SetCratePedestals((0x1<<i),temp_pattern);
            if (errors){
              lprintf("Error setting up pedestals, Slot %d, channel %d.\n",i,j);
              if (errors > MAX_ERRORS){
                lprintf("Too many errors. Exiting\n");
                mtc->DisablePulser();
                mtc->UnsetPedCrateMask(MASKALL);
                mtc->UnsetGTCrateMask(MASKALL);
                return -1;
              }
            }

            contConnection->GetInput(channel_results[j],100);

            for (int k=0;k<strlen(channel_results[j]);k++)
              if (channel_results[j][k] == '\n')
                channel_results[j][k] = '\0';

            if (strncmp(channel_results[j],"quit",4) == 0){
              lprintf("Quitting.\n");
              mtc->DisablePulser();
              xl3s[crateNum]->DeselectFECs();
              return 0;
            }
          } // end pattern mask
        } // end loop over channels

        // update the database
        if (updateDB){
          lprintf("updating the database\n");
          lprintf("updating slot %d\n",i);
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("scan_refl"));
          json_append_member(newdoc,"trig_type",json_mknumber(triggerSelect));

          int passflag = 1;
          JsonNode *all_channels = json_mkarray();
          for (int j=0;j<32;j++){
            JsonNode *one_chan = json_mkobject();
            json_append_member(one_chan,"id",json_mknumber(j));
            if (strlen(channel_results[j]) != 0){
              passflag = 0;
              json_append_member(one_chan,"error",json_mkstring(channel_results[j]));
            }else{
              json_append_member(one_chan,"error",json_mkstring(""));
            }
            json_append_element(all_channels,one_chan);
          }
          json_append_member(newdoc,"channels",all_channels);
          json_append_member(newdoc,"pass",json_mkbool(passflag));
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // Only have to delete the head node
        }
      } // end if slot mask
    } // end loop over slots

    mtc->UnsetGTMask(0xFFFFFFFF);
    mtc->DisablePulser();
    xl3s[crateNum]->DeselectFECs();

    if (errors)
      lprintf("There were %d errors.\n",errors);
    else
      lprintf("No errors.\n");
  }
  catch(const char* s){
    lprintf("ScanReflection: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}
