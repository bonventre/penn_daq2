#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "MbStabilityTest.h"

int MbStabilityTest(int crateNum, uint32_t slotMask, int numPeds, int updateDB, int finalTest)
{
  printf("*** Starting MB Stability Test *********\n");

  uint32_t result;

  char error_history[16][5000];
  char temp_msg[1000];
  int slot_errors[16];
  uint32_t chan_mask_rand[4],pmtword[3];
  uint32_t crate,slot,chan,nc_cc,gt8,gt16,gtword,cmos_es16,cgt_es16,cgt_es8;
  uint32_t fec_diff,nfire,nfire_16,nfire_24,nfire_gtid;
  int num_chan, rd, num_print;

  // zero some stuff
  memset(temp_msg,'\0',1000);
  for (int i=0;i<16;i++){
    slot_errors[i] = 0;
    memset(error_history[i],'\0',5000);
  }
  rd = 0;
  num_chan = 8;
  nfire_16 = 0;
  nfire_24 = 0;
  num_print = 10;
  chan_mask_rand[0] = 0x11111111;
  chan_mask_rand[1] = 0x22222222;
  chan_mask_rand[2] = 0x44444444;
  chan_mask_rand[3] = 0x88888888;

  printf("Crate: %d, slot mask: %04x\n",crateNum,slotMask);

  printf("Channel mask used: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
      chan_mask_rand[0],chan_mask_rand[1],chan_mask_rand[2],chan_mask_rand[3]);
  printf("Going to fire %d times\n",numPeds);
  for (int i=0;i<16;i++)
    sprintf(error_history[i]+strlen(error_history[i]),"Going to fire %d times\n",numPeds);

  try {

    // set up mtc
    mtc->ResetMemory();
    mtc->SetGTCounter(0);
    //if (setup_pedestals(0,25,150,0,(0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB))
    if (mtc->SetupPedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
          (0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB)){
      printf("Error setting up mtc. Exiting\n");
      return -1;
    }

    // set up crate
    for (int i=0;i<16;i++){
      uint32_t select_reg = FEC_SEL*i;
      uint32_t crate_val = crateNum << FEC_CSR_CRATE_OFFSET;
      xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result);
      xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
      xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,crate_val | 0x6,&result);
      xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,crate_val,&result);
      xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result);
    }
    xl3s[crateNum]->DeselectFECs();


    for (nfire=1;nfire<numPeds;nfire++){
      nfire_16++;
      if (nfire_16 == 65535){
        nfire_16 = 0;
        nfire_24++;
      }
      nfire_gtid = nfire_24*0x10000 + nfire_16;

      // for selected slots, set semi-random pattern
      for (int i=0;i<16;i++){
        if (((0x1<<i)& slotMask) && (slot_errors[i] == 0)){
          uint32_t select_reg = FEC_SEL*i;
          //enable pedestals
          xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,chan_mask_rand[rd],&result);
        }
      }
      xl3s[crateNum]->DeselectFECs();
      rd = (rd+1)%4;

      // fire pulser once
      if (nfire == num_print){
        printf("Pulser fired %u times.\n",nfire);
        for (int i=0;i<16;i++){
          if (((0x1<<i) & slotMask) && (slot_errors[i] == 0)){
            sprintf(error_history[i]+strlen(error_history[i]),"Pulser fired %u times.\n",nfire);
          }
        }
        num_print+=10;
      }
      usleep(1);
      mtc->SoftGT();
      usleep(1);

      for (int j=0;j<16;j++){
        if (((0x1<<j) & slotMask) && (slot_errors[j] == 0)){
          uint32_t select_reg = FEC_SEL*j;

          // check fifo diff pointer
          xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&fec_diff);
          fec_diff &= 0x000FFFFF;
          if (fec_diff != num_chan*3){
            sprintf(temp_msg,">>>Error, fec_diff = %d, expected %d\n",fec_diff,num_chan*3);
            sprintf(temp_msg+strlen(temp_msg),">>>testing crate %d, slot %d\n",crateNum,j);
            sprintf(temp_msg+strlen(temp_msg),">>>stopping at pulser iteration %u\n",nfire);
            sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
            printf("%s",temp_msg);
            slot_errors[j] = 1 ;
          }
        }
      }

      for (int j=0;j<16;j++){
        if (((0x1<<j) & slotMask) && (slot_errors[j] == 0)){
          uint32_t select_reg = FEC_SEL*j;

          // readout loop, check fifo again while reading out
          int iter = 0;
          while(3 <= fec_diff){
            iter++;
            if (iter > num_chan*3){
              sprintf(temp_msg,">>>Error, number of FEC reads exceeds %d, aborting!\n",num_chan*3);
              sprintf(temp_msg+strlen(temp_msg),">>>testing crate %d, slot %d\n",crateNum,j);
              sprintf(temp_msg+strlen(temp_msg),">>>stopping at pulser iteration %u\n",nfire);
              sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
              printf("%s",temp_msg);
              slot_errors[j] = 1 ;
              break;
            }

            //read out memory
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,pmtword);
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,pmtword+1);
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,pmtword+2);
            xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&fec_diff);
            fec_diff &= 0x000FFFFF;

            crate = (uint32_t) UNPK_CRATE_ID(pmtword);
            slot = (uint32_t) UNPK_BOARD_ID(pmtword);
            chan = (uint32_t) UNPK_CHANNEL_ID(pmtword);
            gt8 = (uint32_t) UNPK_FEC_GT8_ID(pmtword);
            gt16 = (uint32_t) UNPK_FEC_GT16_ID(pmtword);
            cmos_es16 = (uint32_t) UNPK_CMOS_ES_16(pmtword);
            cgt_es16 = (uint32_t) UNPK_CGT_ES_16(pmtword);
            cgt_es8 = (uint32_t) UNPK_CGT_ES_24(pmtword);
            nc_cc = (uint32_t) UNPK_NC_CC(pmtword);

            // check crate, slot, nc_cc
            if ((crate != crateNum) || (slot != j) || (nc_cc != 0)){
              sprintf(temp_msg,"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),"Crate/slot or Nc_cc error. Pedestal iter: %u\n",nfire);
              sprintf(temp_msg+strlen(temp_msg),"Expected crate,slot,nc_cc: %d %d %d\n",crateNum,j,0);
              sprintf(temp_msg+strlen(temp_msg),"Found crate,slot,chan,nc_cc: %d %d %d %d\n",crate,slot,chan,nc_cc);
              sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
              sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
              sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
              printf("%s",temp_msg);
              slot_errors[j] = 1;
              break;
            }

            // check gt increment
            gtword = gt8*0x10000 + gt16;
            if (gtword != nfire_gtid){
              sprintf(temp_msg,"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),"GT8/16 error, expect GTID: %u \n",nfire_gtid);
              sprintf(temp_msg+strlen(temp_msg),"Crate,slot,chan: %d %d %d\n",crate,slot,chan);
              sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
              sprintf(temp_msg+strlen(temp_msg),"Found gt8, gt16, gtword: %d %d %08x\n",gt8,gt16,gtword);
              sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
              sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
              sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
              printf("%s",temp_msg);
              slot_errors[j] = 1;
              break;
            }


            // check synclear bits
            if ((cmos_es16 == 1) || (cgt_es16 == 1) || (cgt_es8 == 1)){
              if (gt8 != 0) {
                sprintf(temp_msg,"***************************************\n");
                sprintf(temp_msg+strlen(temp_msg),"Synclear error, GTID: %u\n",nfire_gtid);
                sprintf(temp_msg+strlen(temp_msg),"crate, slot, chan: %d %d %d\n",crate,slot,chan);
                sprintf(temp_msg+strlen(temp_msg),"Bundle 0,1,2: 0x%08x 0x%08x 0x%08x\n",pmtword[0],pmtword[1],pmtword[2]);
                sprintf(temp_msg+strlen(temp_msg),"Found cmos_es16,cgt_es16,cgt_es8: %d %d %d\n",cmos_es16,cgt_es16,cgt_es8);
                sprintf(temp_msg+strlen(temp_msg),"***************************************\n");
                sprintf(temp_msg+strlen(temp_msg),">>>Stopping at pulser iteration %u\n",nfire);
                sprintf(error_history[j]+strlen(error_history[j]),"%s",temp_msg);
                printf("%s",temp_msg);
                slot_errors[j] = 1;
                break;
              }
            }

          } // while reading out

          xl3s[crateNum]->DeselectFECs();

        } //end if slot mask
      } // end loop over slots
    } // loop over nfire

    if (updateDB){
      printf("updating the database\n");
      for (int slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          printf("updating slot %d\n",slot);
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("mb_stability_test"));
          json_append_member(newdoc,"printout",json_mkstring(error_history[slot]));
          json_append_member(newdoc,"pass",json_mkbool(!(slot_errors[slot])));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc);
        }
      }
    }


    printf("Ending mb stability test\n");
  }
  catch(const char* s){
    printf("MbStabilityTest: %s\n",s);
  }
  printf("********************************\n");

  return 0;
}

