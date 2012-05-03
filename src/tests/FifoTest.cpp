#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "FifoTest.h"

int FifoTest(int crateNum, uint32_t slotMask, int updateDB, int finalTest)
{
  lprintf("*** Starting Fifo Test *****************\n");


  uint32_t result;

  char error_history[10000];
  char cur_msg[1000];
  int slot_errors;
  uint32_t remainder,diff,write,read;
  uint32_t gt1[16],gt2[16],bundle[12];
  uint32_t *readout_data;
  int gtstofire;

  readout_data = (uint32_t *) malloc( 0x000FFFFF * sizeof(uint32_t));
  if (readout_data == NULL){
    lprintf("Error mallocing. Exiting\n");
    return -1;
  }

  try {
    // set up mtc
    mtc->ResetMemory();
    mtc->SetGTCounter(0);
    //FIXME
    //if (setup_pedestals(0,25,150,0,(0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB))
    if (mtc->SetupPedestals(0,DEFAULT_PED_WIDTH,DEFAULT_GT_DELAY,DEFAULT_GT_FINE_DELAY,
          (0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB)){
      lprintf("Error setting up mtc. Exiting\n");
      free(readout_data);
      return -1;
    }

    // set up crate
    int busErrors = 0;
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        uint32_t select_reg = FEC_SEL*i;
        busErrors += xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0xf,&result);
        busErrors += xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,crateNum<<FEC_CSR_CRATE_OFFSET,&result);
        if (busErrors != 0){
          lprintf("FEC is not responding. Exiting\n");
          free(readout_data);
          return -1;
        }

      }
    }

    // mask in one channel on each fec
    xl3s[crateNum]->SetCratePedestals(slotMask,0x1);

    // check initial conditions
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        CheckFifo(crateNum,i,&diff,error_history);
        // get initial count
        mtc->GetGTCount(&gt1[i]);
      }
    }

    // now pulse the soft gts
    // we will fill the fifos almost to the top
    gtstofire = (0xFFFFF-32)/3;
    lprintf("Now firing %u soft gts.\n",gtstofire);
    int gtcount = 0;
    while (gtcount < gtstofire){
      if (gtstofire - gtcount > 5000){
        mtc->MultiSoftGT(5000);
        gtcount += 5000;
      }else{
        mtc->MultiSoftGT(gtstofire-gtcount);
        gtcount += gtstofire-gtcount;
      }
      if (gtcount%15000 == 0){
        lprintf(".");
        fflush(stdout);
      }
    }

    lprintf("\n");

    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        // zero some stuff
        memset(cur_msg,'\0',1000);
        slot_errors = 0;
        memset(error_history,'\0',10000);

        // get the updated gt count
        mtc->GetGTCount(&gt2[i]);
        sprintf(cur_msg,"Slot %d - Number of GTs fired: %u\n",i,gt2[i]-gt1[i]);
        sprintf(cur_msg+strlen(cur_msg),"Slot %d - GT before: %u, after: %u\n",i,gt1[i],gt2[i]);
        lprintf("%s",cur_msg);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);

        // make sure it matches the number of gts you sent
        CheckFifo(crateNum,i,&diff,error_history);
        if (diff != 3*(gt2[i]-gt1[i])){
          sprintf(cur_msg,"Slot %d - Unexpected number of fifo counts!\n",i);
          sprintf(cur_msg+strlen(cur_msg),"Slot %d - Based on MTCD GTs fired, should be 0x%05x (%u)\n",i,3*(gt2[i]-gt1[i]),3*(gt2[i]-gt1[i]));
          sprintf(cur_msg+strlen(cur_msg),"Slot %d - Based on times looped, should be 0x%05x (%u)\n",i,gtstofire*3,gtstofire*3);
          lprintf("%s",cur_msg);
          sprintf(error_history+strlen(error_history),"%s",cur_msg);
        }

        // turn off all but one slot
        xl3s[crateNum]->SetCratePedestals(0x1<<i,0x1);

        // now pulse the last soft gts to fill fifo to the top
        remainder = diff/3;
        sprintf(cur_msg,"Slot %d - Now firing %d more soft gts\n",i,remainder);
        lprintf("%s",cur_msg);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        gtcount = 0;
        while (gtcount < remainder){
          if (remainder - gtcount > 5000){
            mtc->MultiSoftGT(5000);
            gtcount += 5000;
          }else{
            mtc->MultiSoftGT(remainder-gtcount);
            gtcount += remainder-gtcount;
          }
        }

        CheckFifo(crateNum,i,&diff,error_history);

        // now read out bundles
        for (int j=0;j<12;j++)
          xl3s[crateNum]->RW(READ_MEM + FEC_SEL*i,0x0,&bundle[j]);

        sprintf(cur_msg,"Slot %d - Read out %d longwords (%d bundles)\n",i,12,12/3);
        lprintf("%s",cur_msg);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);

        CheckFifo(crateNum,i,&diff,error_history);
        remainder = diff/3;
        DumpPmtVerbose(12/3,bundle,error_history);

        // check overflow behavior
        sprintf(cur_msg,"Slot %d - Now overfill FEC (firing %d more soft GTs)\n",i,remainder+3);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);
        gtcount = 0;
        remainder+=3;
        while (gtcount < remainder){
          if (remainder - gtcount > 5000){
            mtc->MultiSoftGT(5000);
            gtcount += 5000;
          }else{
            mtc->MultiSoftGT(remainder-gtcount);
            gtcount += remainder-gtcount;
          }
        }

        CheckFifo(crateNum,i,&diff,error_history);
        uint32_t busy_bits,test_id;
        xl3s[crateNum]->RW(CMOS_BUSY_BIT(0) + READ_REG + FEC_SEL*i,0x0,&busy_bits);
        sprintf(cur_msg,"Should see %d cmos busy bits set. Busy bits are -> 0x%04x\n",3,busy_bits & 0x0000FFFF);
        sprintf(cur_msg+strlen(cur_msg),"(Note that there might be one less than expected as it might be caught up in sequencing.)\n");

        xl3s[crateNum]->RW(CMOS_INTERN_TEST(0) + READ_REG + FEC_SEL*i,0x0,&test_id);
        sprintf(cur_msg+strlen(cur_msg),"See if we can read out test reg: 0x%08x\n",test_id);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        // now read out bundles
        for (int j=0;j<12;j++)
          xl3s[crateNum]->RW(READ_MEM + FEC_SEL*i,0x0,&bundle[j]);

        sprintf(cur_msg,"Slot %d - Read out %d longwords (%d bundles). Should have cleared all busy bits\n",i,12,12/3);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        DumpPmtVerbose(12/3,bundle,error_history);
        CheckFifo(crateNum,i,&diff,error_history);

        xl3s[crateNum]->RW(CMOS_BUSY_BIT(0) + READ_REG + FEC_SEL*i,0x0,&busy_bits);
        sprintf(cur_msg,"Should see %d cmos busy bits set. Busy bits are -> 0x%04x\n",0,busy_bits & 0x0000FFFF);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        // read out data and check the stuff around the wrap of the write pointer
        int leftOver = 30;
        sprintf(cur_msg,"Slot %d - Dumping all but the last %d events.\n",i,leftOver);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);
        int count = xl3s[crateNum]->ReadOutBundles(i,readout_data,(0xFFFFF-diff)/3-leftOver,0);
        lprintf("Managed to read out %d bundles\n",count);

        CheckFifo(crateNum,i,&diff,error_history);
        leftOver = (0x000FFFFF-diff)/3;

        sprintf(cur_msg,"Slot %d - Dumping last %d events!\n",i,leftOver);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        if (leftOver > 0xFFFFF/3){
          lprintf("There was an error calculating how much to read out. Will attempt to read everything thats left\n");
          leftOver = 0xFFFFF/3;
        }
        xl3s[crateNum]->ReadOutBundles(i,readout_data,leftOver,0);
        DumpPmtVerbose(leftOver,readout_data,error_history);
        CheckFifo(crateNum,i,&diff,error_history);

        sprintf(cur_msg,"Slot %d - Trying to read past the end... should get %d bus errors\n",i,12);
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        int busErrors = 0;
        for (int j=0;j<12;j++){
          busErrors += xl3s[crateNum]->RW(READ_MEM + FEC_SEL*i,0x0,&bundle[j]);
        }
        if (busErrors){
          sprintf(cur_msg,"Slot %d - Got expected bus errors (%d).\n",i,busErrors);
        }else{
          sprintf(cur_msg,"Slot %d - Error! Read past end!\n",i);
          slot_errors = 1;
        }
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        xl3s[crateNum]->DeselectFECs();

        sprintf(cur_msg,"Finished Slot %d\n",i);
        sprintf(cur_msg+strlen(cur_msg),"**************************************************\n");
        sprintf(error_history+strlen(error_history),"%s",cur_msg);
        lprintf("%s",cur_msg);

        if (updateDB){
          lprintf("updating the database\n");
          lprintf("updating slot %d\n",i);
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("fifo_test"));
          json_append_member(newdoc,"printout",json_mkstring(error_history));
          json_append_member(newdoc,"pass",json_mkbool(~(slot_errors)));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc);
        }

      } // end if slot mask
    } // end loop over slot

    lprintf("Ending fifo test\n");
  }
  catch(const char* s){
    lprintf("FifoTest: %s\n",s);
  }
  free(readout_data);
  lprintf("********************************\n");

  return 0;

}

static void CheckFifo(int crateNum, int slotNum, uint32_t *thediff, char *msg_buff)
{
  uint32_t diff,read,write,select_reg;
  float remainder;
  char msg[5000];
  select_reg = FEC_SEL*slotNum;
  xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&diff);
  xl3s[crateNum]->RW(FIFO_READ_PTR_R + select_reg + READ_REG,0x0,&read);
  xl3s[crateNum]->RW(FIFO_WRITE_PTR_R + select_reg + READ_REG,0x0,&write);
  diff &= 0x000FFFFF;
  read &= 0x000FFFFF;
  write &= 0x000FFFFF;
  remainder = (float) 0x000FFFFF - (float) diff;

  sprintf(msg,"**************************************\n");
  sprintf(msg+strlen(msg),"Fifo diff ptr is %05x\n",diff);
  sprintf(msg+strlen(msg),"Fifo write ptr is %05x\n",write);
  sprintf(msg+strlen(msg),"Fifo read ptr is %05x\n",read);
  sprintf(msg+strlen(msg),"Left over space is %2.1f (%2.1f bundles)\n",remainder,remainder/3.0);
  sprintf(msg+strlen(msg),"Total events in memory is %2.1f.\n",(float) diff / 3.0);
  sprintf(msg_buff+strlen(msg_buff),"%s",msg);
  lprintf("%s",msg);
  *thediff = (uint32_t) remainder;
}

void DumpPmtVerbose(int n, uint32_t *pmt_buf, char* msg_buf)
{
  int i,j;
  char msg[10000];
  memset(msg,'\0',10000);
  for (i=0;i<n*3;i+=3)
  {
    if (!(i%32)){
      sprintf(msg+strlen(msg),"No\tCh\tCell\t\tGT\tQlx\tQhs\tQhl\tTAC\tES\tMC\tLGI\tNC/CC\tCr\tBd\n");
      sprintf(msg+strlen(msg),"--------------------------------------------------------------------------------------\n");
    }
    sprintf(msg+strlen(msg),"% 4d\t%2u\t%2u\t%8u\t%4u\t%4u\t%4u\t%4u\t%u%u%u\t%1u\t%1u\t%1u\t%2u\t%2u\n",
        i/3,
        (uint32_t) UNPK_CHANNEL_ID(pmt_buf+i),
        (uint32_t) UNPK_CELL_ID(pmt_buf+i),
        (uint32_t) UNPK_FEC_GT_ID(pmt_buf+i),
        (uint32_t) UNPK_QLX(pmt_buf+i),
        (uint32_t) UNPK_QHS(pmt_buf+i),
        (uint32_t) UNPK_QHL(pmt_buf+i),
        (uint32_t) UNPK_TAC(pmt_buf+i),
        (uint32_t) UNPK_CMOS_ES_16(pmt_buf+i),
        (uint32_t) UNPK_CGT_ES_16(pmt_buf+i),
        (uint32_t) UNPK_CGT_ES_24(pmt_buf+i),
        (uint32_t) UNPK_MISSED_COUNT(pmt_buf+i),
        (uint32_t) UNPK_LGI_SELECT(pmt_buf+i),
        (uint32_t) UNPK_NC_CC(pmt_buf+i),
        (uint32_t) UNPK_CRATE_ID(pmt_buf+i),
        (uint32_t) UNPK_BOARD_ID(pmt_buf+i));
  }
  sprintf(msg_buf+strlen(msg_buf),"%s",msg);
  lprintf("%s",msg);
}

