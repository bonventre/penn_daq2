#include "XL3PacketTypes.h"
#include "MTCPacketTypes.h"
#include "XL3Registers.h"
#include "MTCRegisters.h"
#include "Globals.h"
#include "Json.h"
#include "UnpackBundles.h"

#include "DB.h"
#include "MTCModel.h"
#include "XL3Model.h"
#include "CGTTest.h"

int CGTTest(int crateNum, uint32_t slotMask, uint32_t channelMask, int updateDB, int finalTest, int ecal)
{
  lprintf("*** Starting CGT Test ******************\n");

  uint32_t result;

  uint32_t bundles[3];
  int crate_id,slot_id,chan_id,nc_id,gt16_id,gt8_id,es16;

  int missing_bundles[16],chan_errors[16][32];
  char error_history[16][5000];
  char cur_msg[1000];
  int max_errors[16];
  uint32_t badchanmask;
  int num_chans;

  // zero some stuff
  memset(cur_msg,'\0',1000);
  num_chans = 0;
  for (int i=0;i<32;i++)
    if ((0x1<<i) & channelMask)
      num_chans++;
  for (int i=0;i<16;i++){
    for (int j=0;j<32;j++)
      chan_errors[i][j] = 0;
    missing_bundles[i] = 0;
    max_errors[i] = 0;
    memset(error_history[i],'\0',5000);
  }

  try{

    // set up mtc
    mtc->ResetMemory();
    mtc->SetGTCounter(0);
    //if (setup_pedestals(0,25,150,0,(0x1<<arg.crate_num)+MSK_TUB,(0x1<<arg.crate_num)+MSK_TUB))
    if (mtc->SetupPedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
          (0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB)){
      lprintf("Error setting up mtc. Exiting\n");
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

    lprintf("Crate number: %d\n"
        "Slot and Channel mask: %08x %08x\n",crateNum,slotMask,channelMask);

    // select desired fecs
    if (xl3s[crateNum]->SetCratePedestals(slotMask,channelMask)){
      lprintf("Error setting up crate for pedestals. Exiting\n");
      return -1;
    }
    xl3s[crateNum]->DeselectFECs();

    uint32_t num_peds = 0xFFFF + 10000;
    lprintf("Going to fire pulser %u times.\n",num_peds);

    XL3Packet packet;
    int total_pulses = 0;
    int numgt = 0;
    // we now send out gts in bunches, checking periodically
    // that we are getting the right count at the fecs
    for (int j=0;j<16;j++){
      // we skip 4999 gtids then check each 5000th one
      if (j != 13)
        numgt = 4999;
      else
        numgt = 534;

      mtc->MultiSoftGT(numgt);

      // now loop over slots and make sure we got all the gts
      for (int i=0;i<16;i++){
        if (((0x1<<i) & slotMask) && (max_errors[i] == 0)){
          xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*i + READ_REG,0x0,&result);
          if ((result & 0x000FFFFF) != numgt*3*num_chans){
            sprintf(cur_msg,"Not enough bundles slot %d: expected %d, found %u\n",
                i,numgt*3*num_chans,result & 0x000FFFFF);
            lprintf("%s",cur_msg);
            if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
              sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            else
              max_errors[i] = 2;
            missing_bundles[i] = 1;
          }

        } // end if in slot mask and not max errors
      } // end loop over slots

      // reset the fifo
      packet.header.packetType = RESET_FIFOS_ID;
      ResetFifosArgs *args = (ResetFifosArgs *) packet.payload;
      args->slotMask = slotMask;
      SwapLongBlock(args,sizeof(ResetFifosArgs)/sizeof(uint32_t));
      xl3s[crateNum]->SendCommand(&packet);
      for (int i=0;i<16;i++)
        if ((0x1<<i) & slotMask && (max_errors[i] == 0))
          xl3s[crateNum]->RW(GENERAL_CSR_R + FEC_SEL*i + WRITE_REG,
              (crateNum << FEC_CSR_CRATE_OFFSET),&result);



      // now send a single soft gt and make sure it looks good
      mtc->SoftGT();

      total_pulses += numgt+1;
      if (j == 13)
        total_pulses++; // rollover bug

      for (int i=0;i<16;i++){
        if (((0x1<<i) & slotMask) && (max_errors[i] == 0)){
          uint32_t select_reg = FEC_SEL*i;
          xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&result);
          if ((result & 0x000FFFFF) != 3*num_chans){
            sprintf(cur_msg,"Not enough bundles slot %d: expected %d, found %u\n",
                i,3*num_chans,result & 0x000FFFFF);
            lprintf("%s",cur_msg);
            if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
              sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
            else
              max_errors[i] = 2;
            missing_bundles[i] = 1;
          }

          // read out one bundle for each channel
          badchanmask = channelMask;
          for (int k=0;k<((result&0x000FFFFF)/3);k++){
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,&bundles[0]);
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,&bundles[1]);
            xl3s[crateNum]->RW(READ_MEM + select_reg,0x0,&bundles[2]);

            crate_id = (int) UNPK_CRATE_ID(bundles); 
            slot_id = (int) UNPK_BOARD_ID(bundles);
            chan_id = (int) UNPK_CHANNEL_ID(bundles);
            nc_id = (int) UNPK_NC_CC(bundles);
            gt16_id = (int) UNPK_FEC_GT16_ID(bundles);
            gt8_id = (int) UNPK_FEC_GT8_ID(bundles);
            es16 = (int) UNPK_CGT_ES_16(bundles);

            badchanmask &= ~(0x1<<chan_id);

            if (crate_id != crateNum){
              sprintf(cur_msg,"Crate wrong for slot %d, chan %u: expected %d, read %u\n",
                  i,chan_id,crateNum,crate_id);
              lprintf("%s",cur_msg);
              if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
              else
                max_errors[i] = 2;
              chan_errors[i][chan_id] = 1;
            } 
            if (slot_id != i){
              sprintf(cur_msg,"Slot wrong for slot %d chan %u: expected %d, read %u\n",
                  i,chan_id,i,slot_id);
              lprintf("%s",cur_msg);
              if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
              else
                max_errors[i] = 2;
              chan_errors[i][chan_id] = 1;
            } 
            if (nc_id != 0x0){
              sprintf(cur_msg,"NC_CC wrong for slot %d chan %u: expected %d, read %u\n",
                  i,chan_id,0,nc_id);
              lprintf("%s",cur_msg);
              if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
              else
                max_errors[i] = 2;
              chan_errors[i][chan_id] = 1;
            } 
            if ((gt16_id + (65536*gt8_id)) != total_pulses){
              if (gt16_id == total_pulses%65536){
                sprintf(cur_msg,"Bad upper 8 Gtid bits for slot %d chan %u: expected %d, read %u\n"
                    "%08x %08x %08x\n",
                    i,chan_id,total_pulses-total_pulses%65536,(65536*gt8_id),bundles[0],
                    bundles[1],bundles[2]);
                lprintf("%s",cur_msg);
                if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                  sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
                else
                  max_errors[i] = 2;

              }else if (gt8_id == total_pulses/65536){
                sprintf(cur_msg,"Bad lower 16 gtid bits for slot %d chan %u: expected %d, read %u\n"
                    "%08x %08x %08x\n",
                    i,chan_id,total_pulses%65536,gt16_id,bundles[0],
                    bundles[1],bundles[2]);
                lprintf("%s",cur_msg);
                if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                  sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
                else
                  max_errors[i] = 2;
              }else{
                sprintf(cur_msg,"Bad gtid for slot %d chan %u: expected %d, read %u\n"
                    "%08x %08x %08x\n",
                    i,chan_id,total_pulses,gt16_id+(65536*gt8_id),bundles[0],
                    bundles[1],bundles[2]);
                lprintf("%s",cur_msg);
                if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                  sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
                else
                  max_errors[i] = 2;
              }
              chan_errors[i][chan_id] = 1;
            } 
            if (es16 != 0x0 && j >= 13){
              sprintf(cur_msg,"Synclear error for slot %d chan %u.\n",
                  i,chan_id);
              lprintf("%s",cur_msg);
              if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
              else
                max_errors[i] = 2;
              chan_errors[i][chan_id] = 1;
            } 
          } // end loop over bundles being read out

          for (int k=0;k<32;k++){
            if ((0x1<<k) & badchanmask){
              sprintf(cur_msg,"No bundle found for slot %d chan %d\n",i,k);
              lprintf("%s",cur_msg);
              if ((strlen(error_history[i]) + strlen(cur_msg)) < sizeof(error_history[i]))
                sprintf(error_history[i]+strlen(error_history[i]),"%s",cur_msg);
              else
                max_errors[i] = 2;
              chan_errors[i][k] = 1;
            }
          }

        } // end if in slot mask and not max errors
      } // end loop over slots

      // check if we should stop any slot
      // because there are too many errors
      for (int i=0;i<16;i++){
        if (((strlen(error_history[i]) > 5000) && (max_errors[i] == 0)) || (max_errors[i] == 2)){
          lprintf("Too many errors slot %d. Skipping that slot\n",i);
          max_errors[i] = 1;
        }
      }

      lprintf("%d pulses\n",total_pulses);
      for (int i=0;i<16;i++)
        sprintf(error_history[i]+strlen(error_history[i]),"%d pulses\n",total_pulses);

    } // end loop over gt bunches

    if (updateDB){
      lprintf("updating the database\n");
      int passflag;
      for (int slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          lprintf("updating slot %d\n",slot);
          passflag = 1;
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("cgt_test"));
          json_append_member(newdoc,"missing_bundles",json_mkbool(missing_bundles[slot]));
          if (missing_bundles[slot] > 0)
            passflag = 0;
          JsonNode *chan_errs = json_mkarray();
          for (int i=0;i<32;i++){
            json_append_element(chan_errs,json_mkbool(chan_errors[slot][i]));
            if (chan_errors[slot][i] > 0)
              passflag = 0;
          }
          json_append_member(newdoc,"errors",chan_errs);
          json_append_member(newdoc,"printout",json_mkstring(error_history[slot]));
          json_append_member(newdoc,"pass",json_mkbool(passflag));

          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc); // only delete the head node
        }
      }
    }

  }
  catch(const char* s){
    lprintf("CGTTest: %s\n",s);
  }

  lprintf("Ending cgt test\n");
  lprintf("****************************************\n");
  return 0;
}


