#include <stdlib.h>
#include "math.h"

#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "PedRun.h"

int PedRun(int crateNum, uint32_t slotMask, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int numPedestals, int upper, int lower, int updateDB, int balanced, int finalTest, int ecal)
{
  lprintf("*** Starting Pedestal Run **************\n");

  lprintf("-------------------------------------------\n");
  lprintf("Crate:		    %2d\n",crateNum);
  lprintf("Slot Mask:		    0x%4x\n",slotMask);
  lprintf("Pedestal Mask:	    0x%08x\n",channelMask);
  lprintf("GT delay (ns):	    %3hu\n", gtDelay);
  lprintf("Pedestal Width (ns):    %2d\n",pedWidth);
  lprintf("Pulser Frequency (Hz):  %3.0f\n",frequency);
  lprintf("Num pedestals:    %d\n",numPedestals);
  lprintf("Lower/Upper pedestal check range: %d %d\n",lower,upper);

  uint32_t *pmt_buffer = (uint32_t *) malloc(0x100000*sizeof(uint32_t));
  if (pmt_buffer == (uint32_t *) NULL){
    lprintf("Problem mallocing!\n");
    return -1;
  }
  struct pedestal *ped = (struct pedestal *) malloc(32*sizeof(struct pedestal)); 
  if (ped == (struct pedestal *) NULL){
    lprintf("Problem mallocing!\n");
    free(pmt_buffer);
    return -1;
  }
  try {

    int num_channels = 0;
    for (int i=0;i<32;i++)
      if ((0x1<<i) & channelMask)
        num_channels++;
    // set up crate
    xl3s[crateNum]->ChangeMode(INIT_MODE,0x0);
        // set up MTC
    int errors = mtc->SetupPedestals(frequency,pedWidth,gtDelay,DEFAULT_GT_FINE_DELAY,
        (0x1<<crateNum),(0x1<<crateNum));
    if (errors){
      lprintf("Error setting up MTC for pedestals. Exiting\n");
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);
      free(pmt_buffer);
      free(ped);
      return -1;
    }

    uint32_t result;
    xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*2 + READ_REG,0x0,&result);
    printf("before reset fifo: %08x\n",result);

    // reset the fifo
    XL3Packet packet;
    packet.header.packetType = RESET_FIFOS_ID;
    ResetFifosArgs *args = (ResetFifosArgs *) packet.payload;
    args->slotMask = slotMask;
    SwapLongBlock(args,sizeof(ResetFifosArgs)/sizeof(uint32_t));
    xl3s[crateNum]->SendCommand(&packet);

    xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*2 + READ_REG,0x0,&result);
    printf("after reset fifo: %08x\n",result);
    /*
    for (int slot=0;slot<16;slot++){
      if ((0x1<<slot) & slotMask){
        uint32_t select_reg = FEC_SEL*slot;
        uint32_t result;
        xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result);
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);
        xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result);
      }
    }
    */
    xl3s[crateNum]->DeselectFECs();
    errors = xl3s[crateNum]->LoadCrateAddr(slotMask);

    xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*2 + READ_REG,0x0,&result);
    printf("after load address: %08x\n",result);


    errors += xl3s[crateNum]->SetCratePedestals(slotMask,channelMask);
    xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*2 + READ_REG,0x0,&result);
    printf("after setting up pedestals: %08x\n",result);
    xl3s[crateNum]->DeselectFECs();
    if (errors){
      lprintf("Error setting up crate for pedestals. Exiting\n");
      free(pmt_buffer);
      free(ped);
      return -1;
    }
    // send pedestals
    uint32_t totalPulses = numPedestals*16;
    uint32_t beforeGT,afterGT;
    mtc->RegRead(MTCOcGtReg,&beforeGT);
    if (frequency == 0){
      int num_to_send = numPedestals*16;
      while (num_to_send > 0){
        if (num_to_send > 1000){
          mtc->MultiSoftGT(1000);
          num_to_send-=1000;
        }else{
          mtc->MultiSoftGT(num_to_send);
          num_to_send = 0;
        }
      }
      //multi_softgt(numPedestals*16);
      mtc->DisablePulser();
    }else{
      float wait_time = (float) numPedestals*16.0/frequency*1E6;
      usleep((int) wait_time);
      mtc->DisablePulser();
      /*
         if (totalPulses%16){
         mtc->MultiSoftGT(16-totalPulses%16);
         totalPulses += 16-totalPulses%16;
         }
         numPedestals = totalPulses/16;
       */
    }
    mtc->RegRead(MTCOcGtReg,&afterGT);
    totalPulses = (afterGT - beforeGT);
    printf("Total pulses: %d (%d bundles)\n",totalPulses,totalPulses*num_channels);
    xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + FEC_SEL*2 + READ_REG,0x0,&result);
    printf("before read out: %08x\n",result);


    // loop over slots
    errors = 0;
    for (int slot=0;slot<16;slot++){
      if ((0x1<<slot) & slotMask){

        // initialize pedestal struct
        for (int i=0;i<32;i++){
          ped[i].channelnumber = i;
          ped[i].per_channel = 0;
          for (int j=0;j<16;j++){
            ped[i].thiscell[j].cellno = j;
            ped[i].thiscell[j].per_cell = 0;
            ped[i].thiscell[j].qlxbar = 0;
            ped[i].thiscell[j].qlxrms = 0;
            ped[i].thiscell[j].qhlbar = 0;
            ped[i].thiscell[j].qhlrms = 0;
            ped[i].thiscell[j].qhsbar = 0;
            ped[i].thiscell[j].qhsrms = 0;
            ped[i].thiscell[j].tacbar = 0;
            ped[i].thiscell[j].tacrms = 0;
          }
        }

        // readout bundles
        int count = xl3s[crateNum]->ReadOutBundles(slot,pmt_buffer,totalPulses*num_channels,0);

        if (count <= 0){
          lprintf("There was an error in the count!\n");
          lprintf("Errors reading out MB %2d (errno %d)\n",slot,count);
          errors++;
          continue;
        }

        lprintf("MB %d: %d bundles read out.\n",slot,count);
        if (count < totalPulses*num_channels)
          errors++;

        // process data
        uint32_t *pmt_iter = pmt_buffer;
        int ch,cell,crateID,num_events;

        for (int i=0;i<count;i++){
          crateID = (int) UNPK_CRATE_ID(pmt_iter);
          if (crateID != crateNum){
            lprintf( "Invalid crate ID seen! (crate ID %2d, bundle %2i)\n",crateID,i);

            pmt_iter+=3;
            continue;
          }
          ch = (int) UNPK_CHANNEL_ID(pmt_iter);
          cell = (int) UNPK_CELL_ID(pmt_iter);
          ped[ch].thiscell[cell].qlxbar += (double) MY_UNPK_QLX(pmt_iter);
          ped[ch].thiscell[cell].qhsbar += (double) UNPK_QHS(pmt_iter);
          ped[ch].thiscell[cell].qhlbar += (double) UNPK_QHL(pmt_iter);
          ped[ch].thiscell[cell].tacbar += (double) UNPK_TAC(pmt_iter);

          ped[ch].thiscell[cell].qlxrms += pow((double) MY_UNPK_QLX(pmt_iter),2.0);
          ped[ch].thiscell[cell].qhsrms += pow((double) UNPK_QHS(pmt_iter),2.0);
          ped[ch].thiscell[cell].qhlrms += pow((double) UNPK_QHL(pmt_iter),2.0);
          ped[ch].thiscell[cell].tacrms += pow((double) UNPK_TAC(pmt_iter),2.0);

          ped[ch].per_channel++;
          ped[ch].thiscell[cell].per_cell++;

          pmt_iter += 3; //increment pointer
        }

        // do final step
        // final step of calculation
        for (int i=0;i<32;i++){
          if (ped[i].per_channel > 0){
            for (int j=0;j<16;j++){
              num_events = ped[i].thiscell[j].per_cell;

              //don't do anything if there is no data here or n=1 since
              //that gives 1/0 below.
              if (num_events > 1){

                // now x_avg = sum(x) / N, so now xxx_bar is calculated
                ped[i].thiscell[j].qlxbar /= num_events;
                ped[i].thiscell[j].qhsbar /= num_events;
                ped[i].thiscell[j].qhlbar /= num_events;
                ped[i].thiscell[j].tacbar /= num_events;

                // now x_rms^2 = n/(n-1) * (<xxx^2>*N/N - xxx_bar^2)
                ped[i].thiscell[j].qlxrms = num_events / (num_events -1)
                  * ( ped[i].thiscell[j].qlxrms / num_events
                      - pow( ped[i].thiscell[j].qlxbar, 2.0));
                ped[i].thiscell[j].qhlrms = num_events / (num_events -1)
                  * ( ped[i].thiscell[j].qhlrms / num_events
                      - pow( ped[i].thiscell[j].qhlbar, 2.0));
                ped[i].thiscell[j].qhsrms = num_events / (num_events -1)
                  * ( ped[i].thiscell[j].qhsrms / num_events
                      - pow( ped[i].thiscell[j].qhsbar, 2.0));
                ped[i].thiscell[j].tacrms = num_events / (num_events -1)
                  * ( ped[i].thiscell[j].tacrms / num_events
                      - pow( ped[i].thiscell[j].tacbar, 2.0));

                // finally x_rms = sqrt(x_rms^2)
                if (ped[i].thiscell[j].qlxrms > 0 || ped[i].thiscell[j].qlxrms == 0)
                  ped[i].thiscell[j].qlxrms = sqrt(ped[i].thiscell[j].qlxrms);
                else
                  ped[i].thiscell[j].qlxrms = -1;
                if (ped[i].thiscell[j].qhsrms > 0 || ped[i].thiscell[j].qhsrms == 0)
                  ped[i].thiscell[j].qhsrms = sqrt(ped[i].thiscell[j].qhsrms);
                else
                  ped[i].thiscell[j].qhsrms = -1;
                if (ped[i].thiscell[j].qhlrms > 0 || ped[i].thiscell[j].qhlrms == 0)
                  ped[i].thiscell[j].qhlrms = sqrt(ped[i].thiscell[j].qhlrms);
                else
                  ped[i].thiscell[j].qhlrms = -1;
                if (ped[i].thiscell[j].tacrms > 0 || ped[i].thiscell[j].tacrms == 0)
                  ped[i].thiscell[j].tacrms = sqrt(ped[i].thiscell[j].tacrms);
                else
                  ped[i].thiscell[j].tacrms = -1;
              }else{
                ped[i].thiscell[j].qlxrms = 0;
                ped[i].thiscell[j].qhsrms = 0;
                ped[i].thiscell[j].qhlrms = 0;
                ped[i].thiscell[j].tacrms = 0;
              }
            }
          }
        }

        // print results
        lprintf("########################################################\n");
        lprintf("Slot (%2d)\n", slot);
        lprintf("########################################################\n");

        uint32_t error_flag[32];

        for (int i = 0; i<32; i++){
          error_flag[i] = 0;
          if ((0x1<<i) & channelMask){
            lprintf("Ch Cell  #   Qhl         Qhs         Qlx         TAC\n");
            for (int j=0;j<16;j++){
              lprintf("%2d %3d %4d %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f\n",
                  i,j,ped[i].thiscell[j].per_cell,
                  ped[i].thiscell[j].qhlbar, ped[i].thiscell[j].qhlrms,
                  ped[i].thiscell[j].qhsbar, ped[i].thiscell[j].qhsrms,
                  ped[i].thiscell[j].qlxbar, ped[i].thiscell[j].qlxrms,
                  ped[i].thiscell[j].tacbar, ped[i].thiscell[j].tacrms);
              if (ped[i].thiscell[j].per_cell < totalPulses/16*.8 || ped[i].thiscell[j].per_cell > totalPulses/16*1.2)
                error_flag[i] |= 0x1;
              if (ped[i].thiscell[j].qhlbar < lower || 
                  ped[i].thiscell[j].qhlbar > upper ||
                  ped[i].thiscell[j].qhsbar < lower ||
                  ped[i].thiscell[j].qhsbar > upper ||
                  ped[i].thiscell[j].qlxbar < lower ||
                  ped[i].thiscell[j].qlxbar > upper)
                error_flag[i] |= 0x2;
              if (ped[i].thiscell[j].tacbar > TACBAR_MAX ||
                  ped[i].thiscell[j].tacbar < TACBAR_MIN)
                error_flag[i] |= 0x4;
              if (ped[i].thiscell[j].qhlrms > 24.0 || 
                  ped[i].thiscell[j].qhsrms > 24.0 ||
                  ped[i].thiscell[j].qlxrms > 24.0 ||
                  ped[i].thiscell[j].tacrms > 100.0)
                error_flag[i] |= 0x8;
            }
            if (error_flag[i] & 0x1)
              lprintf(">>>Wrong no of pedestals for this channel\n");
            if (error_flag[i] & 0x2)
              lprintf(">>>Bad Q pedestal for this channel\n");
            if (error_flag[i] & 0x4)
              lprintf(">>>Bad TAC pedestal for this channel\n");
            if (error_flag[i] & 0x8)
              lprintf(">>>Bad Q RMS pedestal for this channel\n");
          }
        }


        // update database
        if (updateDB){
          lprintf("updating the database\n");
          JsonNode *newdoc = json_mkobject();
          JsonNode *num = json_mkarray();
          JsonNode *qhl = json_mkarray();
          JsonNode *qhlrms = json_mkarray();
          JsonNode *qhs = json_mkarray();
          JsonNode *qhsrms = json_mkarray();
          JsonNode *qlx = json_mkarray();
          JsonNode *qlxrms = json_mkarray();
          JsonNode *tac = json_mkarray();
          JsonNode *tacrms = json_mkarray();
          JsonNode *error_node = json_mkarray();
          JsonNode *error_flags = json_mkarray();
          for (int i=0;i<32;i++){
            JsonNode *numtemp = json_mkarray();
            JsonNode *qhltemp = json_mkarray();
            JsonNode *qhlrmstemp = json_mkarray();
            JsonNode *qhstemp = json_mkarray();
            JsonNode *qhsrmstemp = json_mkarray();
            JsonNode *qlxtemp = json_mkarray();
            JsonNode *qlxrmstemp = json_mkarray();
            JsonNode *tactemp = json_mkarray();
            JsonNode *tacrmstemp = json_mkarray();
            for (int j=0;j<16;j++){
              json_append_element(numtemp,json_mknumber(ped[i].thiscell[j].per_cell));
              json_append_element(qhltemp,json_mknumber(ped[i].thiscell[j].qhlbar));	
              json_append_element(qhlrmstemp,json_mknumber(ped[i].thiscell[j].qhlrms));	
              json_append_element(qhstemp,json_mknumber(ped[i].thiscell[j].qhsbar));	
              json_append_element(qhsrmstemp,json_mknumber(ped[i].thiscell[j].qhsrms));	
              json_append_element(qlxtemp,json_mknumber(ped[i].thiscell[j].qlxbar));	
              json_append_element(qlxrmstemp,json_mknumber(ped[i].thiscell[j].qlxrms));	
              json_append_element(tactemp,json_mknumber(ped[i].thiscell[j].tacbar));	
              json_append_element(tacrmstemp,json_mknumber(ped[i].thiscell[j].tacrms));	
            }
            json_append_element(num,numtemp);
            json_append_element(qhl,qhltemp);
            json_append_element(qhlrms,qhlrmstemp);
            json_append_element(qhs, qhstemp);
            json_append_element(qhsrms, qhsrmstemp);
            json_append_element(qlx, qlxtemp);
            json_append_element(qlxrms, qlxrmstemp);
            json_append_element(tac, tactemp);
            json_append_element(tacrms, tacrmstemp);
            json_append_element(error_node,json_mkbool(error_flag[i]));
            json_append_element(error_flags,json_mknumber((double)error_flag[i]));
          }
          json_append_member(newdoc,"type",json_mkstring("ped_run"));
          json_append_member(newdoc,"num",num);
          json_append_member(newdoc,"qhl",qhl);
          json_append_member(newdoc,"qhl_rms",qhlrms);
          json_append_member(newdoc,"qhs",qhs);
          json_append_member(newdoc,"qhs_rms",qhsrms);
          json_append_member(newdoc,"qlx",qlx);
          json_append_member(newdoc,"qlx_rms",qlxrms);
          json_append_member(newdoc,"tac",tac);
          json_append_member(newdoc,"tac_rms",tacrms);
          json_append_member(newdoc,"errors",error_node);
          json_append_member(newdoc,"error_flags",error_flags);

          int pass_flag = 1;;
          for (int j=0;j<32;j++)
            if (error_flag[j] != 0)
              pass_flag = 0;
          json_append_member(newdoc,"pass",json_mkbool(pass_flag));
          json_append_member(newdoc,"balanced",json_mkbool(balanced));

          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc); // only need to delete the head node
        }

      } // end loop over slot mask
    } // end loop over slots


    // disable triggers
    mtc->UnsetPedCrateMask(MASKALL);
    mtc->UnsetGTCrateMask(MASKALL);

    // turn off pedestals
    xl3s[crateNum]->SetCratePedestals(slotMask,0x0);
    xl3s[crateNum]->DeselectFECs();
    if (errors)
      lprintf("There were %d errors\n",errors);
    else
      lprintf("No errors seen\n");

  }
  catch(const char* s){
    lprintf("PedRun: %s\n",s);
  }
  free(pmt_buffer);
  free(ped);
  lprintf("****************************************\n");

  return 0;
}

