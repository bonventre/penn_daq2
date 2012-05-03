#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MemTest.h"

int MemTest(int crateNum, int slotNum, int updateDB, int finalTest)
{
  lprintf("*** Starting Mem Test ******************\n");
  XL3Packet packet;
  packet.header.packetType = MEM_TEST_ID;
  MemTestArgs *args = (MemTestArgs *) packet.payload;
  MemTestResults *results = (MemTestResults *) packet.payload;
  args->slotNum = slotNum;
  SwapLongBlock(packet.payload,sizeof(MemTestArgs)/sizeof(uint32_t));
  try{
    lprintf("Getting crate configuration\n");
    xl3s[crateNum]->UpdateCrateConfig(0x1<<slotNum);
    lprintf("Starting Mem test\n");
    xl3s[crateNum]->SendCommand(&packet,1,30);
    SwapLongBlock(packet.payload,sizeof(MemTestResults)/sizeof(uint32_t));
    
    if (updateDB){
      lprintf("updating the database\n");
      char hextostr[50];
      JsonNode *newdoc = json_mkobject();
      json_append_member(newdoc,"type",json_mkstring("mem_test"));

      // results of address test, which address bits are broken
      JsonNode* address_test = json_mkarray();
      for (int i=0;i<20;i++){
        json_append_element(address_test,
            json_mkbool(!(results->addressBitFailures & (0x1<<i))));
      }
      json_append_member(newdoc,"address_test_ok",address_test);

      // results of pattern test, where first error was
      JsonNode* pattern_test = json_mkobject();
      json_append_member(pattern_test,"error",json_mkbool(results->errorLocation!=0xFFFFFFFF));
      sprintf(hextostr,"%08x",results->errorLocation);
      json_append_member(pattern_test,"error_location",json_mkstring(hextostr));
      sprintf(hextostr,"%08x",results->expectedData);
      json_append_member(pattern_test,"expected_data",json_mkstring(hextostr));
      sprintf(hextostr,"%08x",results->readData);
      json_append_member(pattern_test,"read_data",json_mkstring(hextostr));
      json_append_member(newdoc,"pattern_test",pattern_test);

      json_append_member(newdoc,"pass",
          json_mkbool((results->addressBitFailures == 0x0) &&
            (results->errorLocation == 0xFFFFFFFF)));

      if (finalTest)
        json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slotNum]));	

      PostDebugDoc(crateNum,slotNum,newdoc,0);
      json_delete(newdoc); // Only have to delete the head node
    }
  }
  catch(const char* s){
    lprintf("MemTest: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}


