#include <stdlib.h>

#include "XL3PacketTypes.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "CaldTest.h"

int CaldTest(int crateNum, uint32_t slotMask, int upper, int lower, int numPoints, int samples, int updateDB, int finalTest)
{
  lprintf("*** Starting Cal Dac Test **************\n");
  lprintf(" (Make sure you have initialized with the caldac xilinx code first!\n");

  uint16_t *point_buf;
  uint16_t *adc_buf;
  point_buf = (uint16_t *) malloc(16*MAX_SAMPLES*sizeof(uint16_t));
  adc_buf = (uint16_t *) malloc(16*4*MAX_SAMPLES*sizeof(uint16_t));
  if ((point_buf == NULL) || (adc_buf == NULL)){
    lprintf("Problem mallocing for cald test. Exiting\n");
    return -1;
  }
  memset(adc_buf,0,16*4*MAX_SAMPLES*sizeof(uint16_t));
  memset(point_buf,0,16*MAX_SAMPLES*sizeof(uint16_t));

  int num_slots = 0;
  for (int i=0;i<16;i++)
    if ((0x1<<i) & slotMask)
      num_slots++;


  XL3Packet packet;
  memset(&packet, 0, sizeof(XL3Packet));
  packet.header.packetType = CALD_TEST_ID;
  CaldTestArgs *args = (CaldTestArgs *) packet.payload;

  args->slotMask = slotMask;
  args->numPoints = numPoints;
  args->samples = samples;
  args->upper = upper;
  args->lower = lower;
  SwapLongBlock(args,sizeof(CaldTestArgs)/sizeof(uint32_t));

  try{
    xl3s[crateNum]->SendCommand(&packet,0,20);
    int total_points = xl3s[crateNum]->GetCaldTestResults(point_buf,adc_buf);

    lprintf("Got results of cald test. %d points received.\n",total_points);
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        int iter = 0;
        while(iter<=MAX_SAMPLES && iter < total_points){
          if (iter != 0 && point_buf[i*MAX_SAMPLES+iter] == 0)
            break;
          lprintf("Slot %d - %u : %4u %4u %4u %4u\n",i,point_buf[i*MAX_SAMPLES+iter],adc_buf[i*4*MAX_SAMPLES+iter*4],adc_buf[i*4*MAX_SAMPLES+iter*4+1],adc_buf[i*4*MAX_SAMPLES+iter*4+2],adc_buf[i*4*MAX_SAMPLES+iter*4+3]);
          iter++;
        }
      }
    }

    if (updateDB){
      lprintf("updating database\n");
      for (int i=0;i<16;i++){
        if ((0x1<<i) & slotMask){
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("cald_test"));
          JsonNode *points = json_mkarray();
          JsonNode *adc0 = json_mkarray();
          JsonNode *adc1 = json_mkarray();
          JsonNode *adc2 = json_mkarray();
          JsonNode *adc3 = json_mkarray();
          int iter = 0;
          while(iter<=MAX_SAMPLES && iter < total_points){
            if (iter != 0 && point_buf[i*MAX_SAMPLES+iter] == 0)
              break;
            json_append_element(points,json_mknumber((double)point_buf[i*MAX_SAMPLES+iter]));
            json_append_element(adc0,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4]));
            json_append_element(adc1,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+1]));
            json_append_element(adc2,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+2]));
            json_append_element(adc3,json_mknumber((double)adc_buf[i*4*MAX_SAMPLES+iter*4+3]));
            iter++;
          }
          json_append_member(newdoc,"dac_value",points);
          json_append_member(newdoc,"adc_0",adc0);
          json_append_member(newdoc,"adc_1",adc1);
          json_append_member(newdoc,"adc_2",adc2);
          json_append_member(newdoc,"adc_3",adc3);
          json_append_member(newdoc,"pass",json_mkbool(1)); //FIXME
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // only delete the head node
        }
      }
    }

    free(point_buf);
    free(adc_buf);

  }
  catch(const char* s){
    lprintf("CaldTest: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}


