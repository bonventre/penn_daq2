#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "ZDisc.h"

int ZDisc(int crateNum, uint32_t slotMask, float rate, int offset, int updateDB, int finalTest, int ecal)
{
  printf("*** Starting Zero Discriminator ********\n");

  printf("Desired rate:\t %5.1f\n",rate);
  printf("Offset:\t %d\n",offset);

  try {

    XL3Packet packet;
    ZDiscArgs *args = (ZDiscArgs *) packet.payload;
    ZDiscResults *results = (ZDiscResults *) packet.payload;

    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        // tell xl3 to zdisc
        packet.header.packetType = ZDISC_ID;
        args->slotNum = i;
        args->offset = offset;
        args->rate = rate;
        SwapLongBlock(args,sizeof(ZDiscArgs)/sizeof(uint32_t));
        xl3s[crateNum]->SendCommand(&packet,1,60);
        SwapLongBlock(results,97); // not all is uint32_ts

        // printout
        printf("channel    max rate,       lower,       upper\n");
        printf("------------------------------------------\n");
        for (int j=0;j<32;j++)
          printf("ch (%2d)   %5.2f(MHz)  %6.1f(KHz)  %6.1f(KHz)\n",
              j,results->maxRate[j]/1E6,results->lowerRate[j]/1E3,
              results->upperRate[j]/1E3);
        printf("Dac Settings\n");
        printf("channel     Max   Lower   Upper   U+L/2\n");
        for (int j=0;j<32;j++)
        {
          printf("ch (%2i)   %5hu   %5hu   %5hu   %5hu\n",
              j,results->maxDacSetting[j],results->lowerDacSetting[j],
              results->upperDacSetting[j],results->zeroDacSetting[j]);
          if (results->lowerDacSetting[j] > results->maxDacSetting[j])
            printf(" <- lower > max! (MaxRate(MHz):%5.2f, lowrate(KHz):%5.2f\n",
                results->maxRate[j]/1E6,results->lowerRate[j]/1E3);
        }

        // update the database
        if (updateDB){
          printf("updating the database\n");
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("zdisc"));

          JsonNode *maxratenode = json_mkarray();
          JsonNode *lowerratenode = json_mkarray();
          JsonNode *upperratenode = json_mkarray();
          JsonNode *maxdacnode = json_mkarray();
          JsonNode *lowerdacnode = json_mkarray();
          JsonNode *upperdacnode = json_mkarray();
          JsonNode *zerodacnode = json_mkarray();
          JsonNode *errorsnode = json_mkarray();
          int passflag = 1;
          for (int j=0;j<32;j++){
            json_append_element(maxratenode,json_mknumber(results->maxRate[j]));	
            json_append_element(lowerratenode,json_mknumber(results->lowerRate[j]));	
            json_append_element(upperratenode,json_mknumber(results->upperRate[j]));	
            json_append_element(maxdacnode,json_mknumber((double)results->maxDacSetting[j]));	
            json_append_element(lowerdacnode,json_mknumber((double)results->lowerDacSetting[j]));	
            json_append_element(upperdacnode,json_mknumber((double)results->upperDacSetting[j]));	
            json_append_element(zerodacnode,json_mknumber((double)results->zeroDacSetting[j]));	
            if (results->maxRate[j] == 0 || results->lowerRate[j] == 0 || results->upperRate[j] == 0 || results->zeroDacSetting[j] == 255){
              passflag = 0;
              json_append_element(errorsnode,json_mkbool(1));	
            }else{
              json_append_element(errorsnode,json_mkbool(0));	
            }
          }
          json_append_member(newdoc,"max_rate",maxratenode);
          json_append_member(newdoc,"lower_rate",lowerratenode);
          json_append_member(newdoc,"upper_rate",upperratenode);
          json_append_member(newdoc,"max_dac",maxdacnode);
          json_append_member(newdoc,"lower_dac",lowerdacnode);
          json_append_member(newdoc,"upper_dac",upperdacnode);
          json_append_member(newdoc,"zero_dac",zerodacnode);
          json_append_member(newdoc,"target_rate",json_mknumber(rate));
          json_append_member(newdoc,"errors",errorsnode);
          json_append_member(newdoc,"pass",json_mkbool(passflag));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	

          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // Only need to delete the head node);
        }


      } // end if in slot mask
    } // end loop over slots

    printf("Zero Discriminator complete.\n");

  }
  catch(const char* s){
    printf("ZDisc: %s\n",s);
  }

  printf("****************************************\n");
  return 0;
}

