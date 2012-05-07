#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "DiscCheck.h"

int DiscCheck(int crateNum, uint32_t slotMask, int numPeds, int updateDB, int finalTest, int ecal)
{
  lprintf("*** Starting Discriminator Check *******\n");


  int errors, slot;
  uint32_t ctemp, count_temp[8][32],count_i[16][32],count_f[16][32],cdiff;
  int chan_errors[16][32];
  int chan_diff[16][32];
  errors = 0;
  uint32_t result;


  try {
    // set up mtc
    mtc->ResetMemory();
    mtc->SetGTCounter(0);
    //if (setup_pedestals(0,25,150,0,(0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB))
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
      xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
    }
    xl3s[crateNum]->DeselectFECs();

    // get initial data
    uint16_t mask1 = slotMask & 0xFF;
    uint16_t mask2 = slotMask & 0xFF00;
    result = xl3s[crateNum]->GetCmosTotalCount(mask1,count_temp);
    int slot_count = 0;
    for (int i=0;i<8;i++)
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++){
          count_i[i][j] = count_temp[slot_count][j];
        }
        slot_count++;
      }
    result = xl3s[crateNum]->GetCmosTotalCount(mask2,count_temp);
    slot_count = 0;
    for (int i=8;i<16;i++)
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++)
          count_i[i][j] = count_temp[slot_count][j];
        slot_count++;
      }

    // fire pedestals
    int p = numPeds;
    while (p>0){
      if (p > MAX_PER_PACKET){
        mtc->MultiSoftGT(MAX_PER_PACKET);
        p -= MAX_PER_PACKET;
      }else{
        mtc->MultiSoftGT(p);
        p = 0;
      }
      if (p%50000 == 0)
        lprintf("%d\n",p);
    }

    // get final data
    result = xl3s[crateNum]->GetCmosTotalCount(mask1,count_temp);
    slot_count = 0;
    for (int i=0;i<8;i++)
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++)
          count_f[i][j] = count_temp[slot_count][j];
        slot_count++;
      }
    result = xl3s[crateNum]->GetCmosTotalCount(mask2,count_temp);
    slot_count = 0;
    for (int i=8;i<16;i++)
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++)
          count_f[i][j] = count_temp[slot_count][j];
        slot_count++;
      }

    // flag bad channels
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++){
          chan_errors[i][j] = 0;
          chan_diff[i][j] = 0;
          cdiff = count_f[i][j] - count_i[i][j];
          if (cdiff != numPeds){
            lprintf("cmos_count != nped for slot %d chan %d. Nped: %d, cdiff: (%d - %d) %d\n",
                i,j,numPeds,count_f[i][j],count_i[i][j],cdiff);
            chan_errors[i][j] = 1;
            chan_diff[i][j] = cdiff-numPeds;
            errors++;
          }
        }
      }
    }

    lprintf("Disc check complete!\n");

    if (updateDB){
      lprintf("updating the database\n");
      for (int slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("disc_check"));
          int passflag = 1;

          JsonNode *channels = json_mkarray();
          for (int i=0;i<32;i++){
            JsonNode *one_chan = json_mkobject();
            json_append_member(one_chan,"id",json_mknumber((double) i));
            json_append_member(one_chan,"count_minus_peds",json_mknumber((double)chan_diff[slot][i]));
            json_append_member(one_chan,"errors",json_mkbool(chan_errors[slot][i]));
            if (chan_errors[slot][i] > 0)
              passflag = 0;
            json_append_element(channels,one_chan);
          }
          json_append_member(newdoc,"channels",channels);

          json_append_member(newdoc,"pass",json_mkbool(passflag));

          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc); // only need to delete the head node
        }
      }
    }

  }
  catch(const char* s){
    lprintf("DiscCheck: %s\n",s);
  }
  lprintf("****************************************\n");
  return 0;
}

