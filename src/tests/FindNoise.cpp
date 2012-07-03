#include <stdlib.h>
#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "DacNumber.h"
#include "Globals.h"
#include "Json.h"
#include "Pouch.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "FindNoise.h"



int FindNoise(uint32_t crateMask, uint32_t *slotMasks, float frequency, int useDebug, int updateDB, int ecal)
{
  lprintf("*** Starting Noise Run *****************\n");
  lprintf("All crates and mtcs should have been inited with proper values already\n");

  uint32_t *vthr_zeros = (uint32_t *) malloc(sizeof(uint32_t) * 10000);
  char get_db_address[500];
  if (useDebug){
    // use zdisc debug values
    for (int i=0;i<19;i++){
      if ((0x1<<i) & crateMask){
        xl3s[i]->UpdateCrateConfig(slotMasks[i]);
        for (int j=0;j<16;j++){
          if ((0x1<<j) & slotMasks[i]){
            char configString[500];
            sprintf(configString,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
                xl3s[i]->GetMBID(j),xl3s[i]->GetDBID(j,0),
                xl3s[i]->GetDBID(j,1),xl3s[i]->GetDBID(j,2),
                xl3s[i]->GetDBID(j,3));
            sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
                DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
            pouch_request *zdisc_response = pr_init();
            pr_set_method(zdisc_response, GET);
            pr_set_url(zdisc_response, get_db_address);
            pr_do(zdisc_response);
            if (zdisc_response->httpresponse != 200){
              lprintf("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
              free(vthr_zeros);
              return -1;
            }
            JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
            JsonNode *viewrows = json_find_member(viewdoc,"rows");
            int n = json_get_num_mems(viewrows);
            if (n == 0){
              lprintf("Crate %d Slot %d: No zdisc documents for this configuration (%s). Exiting\n",i,j,configString);
              free(vthr_zeros);
              return -1;
            }
            JsonNode *zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode *zero_dac = json_find_member(zdisc_doc,"zero_dac");
            for (int k=0;k<32;k++){
              vthr_zeros[i*32*16+j*32+k] = json_get_number(json_find_element(zero_dac,k));
            }
            json_delete(viewdoc);
            pr_free(zdisc_response);
          }
        }
      }
    }
  }else{
    // use the ECAL values
    for (int i=0;i<19;i++){
      if ((0x1<<i) & crateMask){
        for (int j=0;j<16;j++){
          if ((0x1<<j) & slotMasks[i]){
            sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,%d,\"\"]&endkey=[%d,%d]&descending=true",
                FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,i,j+1,i,j);
            pouch_request *zdisc_response = pr_init();
            pr_set_method(zdisc_response, GET);
            pr_set_url(zdisc_response, get_db_address);
            pr_do(zdisc_response);
            if (zdisc_response->httpresponse != 200){
              lprintf("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
              free(vthr_zeros);
              return -1;
            }
            JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
            JsonNode *viewrows = json_find_member(viewdoc,"rows");
            int n = json_get_num_mems(viewrows);
            if (n == 0){
              lprintf("Crate %d Slot %d: No FEC document. Exiting\n",i,j);
              free(vthr_zeros);
              return -1;
            }
            JsonNode *zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode *hw = json_find_member(zdisc_doc,"hw");
            JsonNode *zero_dac = json_find_member(zdisc_doc,"zero_dac");
            for (int k=0;k<32;k++){
              vthr_zeros[i*32*16+j*32+k] = json_get_number(json_find_element(zero_dac,k));
            }
            json_delete(viewdoc);
            pr_free(zdisc_response);
          }
        }
      }
    }
  } // finished getting vthr_zero values from database

  int threshRange = MAX_THRESH - STARTING_THRESH + 1; 
  uint32_t slot_nums[50];
  uint32_t dac_nums[50];
  uint32_t dac_values[50];
  for (int i=0;i<32;i++){
    dac_nums[i] = d_vthr[i];
    dac_values[i] = 255;
  }

  uint32_t *base_noise = (uint32_t *) malloc(sizeof(uint32_t) * 500000);
  uint32_t *readout_noise = (uint32_t *) malloc(sizeof(uint32_t) * 500000);

  // set up mtcd for pulsing continuously
  if (mtc->SetupPedestals(frequency,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
      crateMask,crateMask)){
    lprintf("Error setting up MTC. Exiting\n");
    free(base_noise);
    free(readout_noise);
    free(vthr_zeros);
    return -1;
  }

  // set all vthr dacs to 255 in all crates all slots
  for (int i=0;i<19;i++){
    if ((0x1<<i) & crateMask){
      for (int j=0;j<16;j++){
        if ((0x1<<j) & slotMasks[i]){
          for (int k=0;k<32;k++)
            slot_nums[k] = j;
          if (xl3s[i]->MultiLoadsDac(32,dac_nums,dac_values,slot_nums)){
            lprintf("Error loading dacs. Exiting\n");
            free(base_noise);
            free(readout_noise);
            free(vthr_zeros);
            return -1;
          }
        }
      }
    }
  }

  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      xl3s[i]->SetCratePedestals(0xFFFF,0xFFFFFFFF);


  uint32_t done_mask[19*16];
  uint32_t total_count1[8][32];
  uint32_t total_count2[8][32];
  uint32_t mycount[3];
  uint32_t result;
  
  // figure out which boards are there and talking to us
  uint32_t existMask[19];
  for (int i=0;i<19;i++){
    if ((0x1<<i) & crateMask){
      existMask[i] = slotMasks[i];
      for (int j=0;j<16;j++){
        xl3s[i]->RW(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,0xFFFFFFFF,&result);
        if (result == 0x0001ABCD)
          existMask[i] |= 0x1<<j;
      }

      XL3Packet packet;
      packet.header.packetType = RESET_FIFOS_ID;
      ResetFifosArgs *args = (ResetFifosArgs *) packet.payload;
      args->slotMask = existMask[i];
      SwapLongBlock(args,sizeof(ResetFifosArgs)/sizeof(uint32_t));
      xl3s[i]->SendCommand(&packet);
    }
  }

  usleep(5000);

  // loop over channels
  for (int k=0;k<32;k++){
    for (int i=0;i<19*16;i++){
      done_mask[i] = 0x0;
    }

    // set pedestal masks (remove just the channel we are working on)
    lprintf("Chan %d\n",k);
    for (int i=0;i<19;i++){
      if ((0x1<<i) & crateMask){
        xl3s[i]->SetCratePedestals(0xFFFF,~(0x1<<k));
        xl3s[i]->ChangeMode(NORMAL_MODE,existMask[i]);
      }
    }
    
    int threshAboveZero = STARTING_THRESH;
    uint32_t crateDoneMask = 0x0;
    do {
      int iter = threshAboveZero - STARTING_THRESH;
      for (int i=0;i<19;i++){
        if ((0x1<<i) & crateMask){
          int numDacs = 0;
          for (int j=0;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              slot_nums[numDacs] = j;
              dac_nums[numDacs] = d_vthr[k];
              dac_values[numDacs] = vthr_zeros[i*16*32+j*32+k] + threshAboveZero;
              numDacs++;
            }
          }
          if (xl3s[i]->MultiLoadsDac(numDacs,dac_nums,dac_values,slot_nums)){
            lprintf("Error loading dacs. Exiting\n");
            free(base_noise);
            free(readout_noise);
            free(vthr_zeros);
            return -1;
          }
          
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF,total_count1);
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF00,total_count2);
          int slotIter = 0;
          for (int j=0;j<8;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                readout_noise[apos] = total_count1[slotIter][k];
              }
              slotIter++;
            }
          }
          slotIter = 0;
          for (int j=8;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                readout_noise[apos] = total_count2[slotIter][k];
              }
              slotIter++;
            }
          }

        }
      } // end loop over crates

      usleep(SLEEP_TIME);

      for (int i=0;i<19;i++){
        if ((0x1<<i) & crateMask){
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF,total_count1);
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF00,total_count2);
          int slotIter = 0;
          for (int j=0;j<8;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                readout_noise[apos] = total_count1[slotIter][k] - readout_noise[apos];
                if (readout_noise[apos] == 0 && threshAboveZero > 0){
                  done_mask[i*16+j] |= (0x1<<k);
                  lprintf("%d %d %d: zero: %d, above: %d, tot: %d\n",i,j,k,vthr_zeros[i*16*32+j*32+k],threshAboveZero,vthr_zeros[i*16*32+j*32+k]+threshAboveZero);
                }
              }
              slotIter++;
            }
          }
          slotIter = 0;
          for (int j=8;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                readout_noise[apos] = total_count2[slotIter][k] - readout_noise[apos];
                if (readout_noise[apos] == 0 && threshAboveZero > 0){
                  done_mask[i*16+j] |= (0x1<<k);
                  lprintf("%d %d %d: zero: %d, above: %d, tot: %d\n",i,j,k,vthr_zeros[i*16*32+j*32+k],threshAboveZero,vthr_zeros[i*16*32+j*32+k]+threshAboveZero);
                }
              }
              slotIter++;
            }
          }

          uint32_t slotDoneMask = 0x0;
          for (int j=0;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (done_mask[i*16+j] == 0xFFFFFFFF)
                slotDoneMask |= (0x1<<j);
            }
          }
          if (slotDoneMask == slotMasks[i])
            crateDoneMask |= (0x1<<i);
        }
      } // end loop over crates

    }while((++threshAboveZero <= MAX_THRESH) && (crateDoneMask != crateMask));

    // now we do it again in init mode for noise without readout
    for (int i=0;i<19*16;i++)
      done_mask[i] = 0x0;

    for (int i=0;i<19;i++){
      if ((0x1<<i) & crateMask){
        xl3s[i]->ChangeMode(INIT_MODE,existMask[i]);
      }
    }
    
    threshAboveZero = STARTING_THRESH;
    crateDoneMask = 0x0;
    do {
      int iter = threshAboveZero - STARTING_THRESH;
      for (int i=0;i<19;i++){
        if ((0x1<<i) & crateMask){
          int numDacs = 0;
          for (int j=0;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              slot_nums[numDacs] = j;
              dac_nums[numDacs] = d_vthr[k];
              dac_values[numDacs] = vthr_zeros[i*16*32+j*32+k] + threshAboveZero;
              numDacs++;
            }
          }
          if (xl3s[i]->MultiLoadsDac(numDacs,dac_nums,dac_values,slot_nums)){
            lprintf("Error loading dacs. Exiting\n");
            free(base_noise);
            free(readout_noise);
            free(vthr_zeros);
            return -1;
          }
          
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF,total_count1);
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF00,total_count2);
          int slotIter = 0;
          for (int j=0;j<8;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                base_noise[apos] = total_count1[slotIter][k];
              }
              slotIter++;
            }
          }
          slotIter = 0;
          for (int j=8;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                base_noise[apos] = total_count2[slotIter][k];
              }
              slotIter++;
            }
          }

        }
      } // end loop over crates

      usleep(SLEEP_TIME);

      for (int i=0;i<19;i++){
        if ((0x1<<i) & crateMask){
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF,total_count1);
          xl3s[i]->GetCmosTotalCount(slotMasks[i] & 0xFF00,total_count2);
          int slotIter = 0;
          for (int j=0;j<8;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                base_noise[apos] = total_count1[slotIter][k] - base_noise[apos];
                if (base_noise[apos] == 0 && threshAboveZero > 0){
                  done_mask[i*16+j] |= (0x1<<k);
                }
              }
              slotIter++;
            }
          }
          slotIter = 0;
          for (int j=8;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (!((0x1<<k) & done_mask[i*16+j])){
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + iter;
                base_noise[apos] = total_count2[slotIter][k] - base_noise[apos];
                if (base_noise[apos] == 0 && threshAboveZero > 0){
                  done_mask[i*16+j] |= (0x1<<k);
                }
              }
              slotIter++;
            }
          }

          uint32_t slotDoneMask = 0x0;
          for (int j=0;j<16;j++){
            if ((0x1<<j) & slotMasks[i]){
              if (done_mask[i*16+j] == 0xFFFFFFFF)
                slotDoneMask |= (0x1<<j);
            }
          }
          if (slotDoneMask == slotMasks[i])
            crateDoneMask |= (0x1<<i);
        }
      } // end loop over crates

    }while((++threshAboveZero <= MAX_THRESH) && (crateDoneMask != crateMask));

  } // end loop over channels

  if (updateDB){
    lprintf("Updating the database\n");
    for (int i=0;i<19;i++){
      if ((0x1<<i) & crateMask){
        for (int j=0;j<16;j++){
          if ((0x1<<j) & slotMasks[i]){
            JsonNode *newdoc = json_mkobject();
            json_append_member(newdoc,"type",json_mkstring("find_noise"));
            JsonNode *channels = json_mkarray();
            for (int k=0;k<32;k++){
              JsonNode *one_chan = json_mkobject();
              json_append_member(one_chan,"id",json_mknumber(k));
              json_append_member(one_chan,"zero_used",json_mknumber(vthr_zeros[i*32*16+j*32+k]));
              JsonNode *points = json_mkarray();
              int finished = 0;
              for (int l=0;l<threshRange;l++){
                JsonNode *one_point = json_mkobject();
                json_append_member(one_point,"thresh_above_zero",json_mknumber(l+STARTING_THRESH));
                int apos = i*16*32*threshRange + j*32*threshRange + k*threshRange + l;
                json_append_member(one_point,"base_noise",json_mknumber(base_noise[apos]));
                json_append_member(one_point,"readout_noise",json_mknumber(readout_noise[apos]));
                json_append_element(points,one_point);
                if (readout_noise[apos] == 0 && (l+STARTING_THRESH) > 0)
                  break;
              }
              json_append_member(one_chan,"points",points);
              json_append_element(channels,one_chan);
            }
            json_append_member(newdoc,"channels",channels);
            if (ecal)
              json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));
            json_append_member(newdoc,"pass",json_mkbool(1)); //FIXME
            
            PostDebugDoc(i,j,newdoc);
            json_delete(newdoc);
          }
        }
      }
    }
  }

  mtc->DisablePulser();
  free(base_noise);
  free(readout_noise);
  free(vthr_zeros);
  lprintf("Finished Find noise\n");

  return 0;
}
