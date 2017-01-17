#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "FECTest.h"

int FECTest(int crateNum, uint32_t slotMask, int updateDB, int finalTest, int ecal)
{
  lprintf("*** Starting FEC Test ******************\n");
  XL3Packet packet;
  memset(&packet, 0, sizeof(XL3Packet));
  packet.header.packetType = FEC_TEST_ID;
  FECTestArgs *args = (FECTestArgs *) packet.payload;
  FECTestResults *results = (FECTestResults *) packet.payload;
  args->slotMask = slotMask;
  SwapLongBlock(packet.payload,sizeof(FECTestArgs)/sizeof(uint32_t));
  try{
    xl3s[crateNum]->SendCommand(&packet);
    SwapLongBlock(packet.payload,sizeof(FECTestResults)/sizeof(uint32_t));

    if (updateDB){
      lprintf("updating the database\n");
      for (int slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          lprintf("updating slot %d\n",slot);
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("fec_test"));
          json_append_member(newdoc,"pedestal",
              json_mkbool(!(results->discreteRegErrors[slot] & 0x1)));
          json_append_member(newdoc,"chip_disable",
              json_mkbool(!(results->discreteRegErrors[slot] & 0x2)));
          json_append_member(newdoc,"lgi_select",
              json_mkbool(!(results->discreteRegErrors[slot] & 0x4)));
          json_append_member(newdoc,"cmos_prog_low",
              json_mkbool(!(results->discreteRegErrors[slot] & 0x8)));
          json_append_member(newdoc,"cmos_prog_high",
              json_mkbool(!(results->discreteRegErrors[slot] & 0x10)));
          JsonNode *cmos_test_array = json_mkarray();
          for (int i=0;i<32;i++){
            json_append_element(cmos_test_array,
                json_mkbool(!(results->cmosTestRegErrors[slot] & (0x1<<i))));
          }
          json_append_element(cmos_test_array,
              json_mkbool(results->cmosTestRegErrors[slot] == 0x0));
          json_append_member(newdoc,"cmos_test_reg",cmos_test_array);
          json_append_member(newdoc,"pass",
              json_mkbool((results->discreteRegErrors[slot] == 0x0) 
                && (results->cmosTestRegErrors[slot] == 0x0)));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc); // Only have to delete the head node
        }
      }
    }



  }
  catch(const char* s){
    lprintf("FECTest: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}


