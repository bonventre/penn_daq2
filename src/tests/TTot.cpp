#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "DacNumber.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "TTot.h"

int GetTTot(int crateNum, uint32_t slotMask, int targetTime, int updateDB, int finalTest, int ecal)
{
  lprintf("*** Starting Get TTot ******************\n");


  uint16_t times[32*16];
  int tot_errors[16][32];

  try {

    // setup the mtc with the triggers going to the TUB
    int errors = mtc->SetupPedestals(0,60,100,0,(0x1<<crateNum) | MSK_CRATE21,MSK_CRATE21);

    int result = MeasureTTot(crateNum,slotMask,150,times);

    // print out results
    lprintf("Crate\t Slot\t Channel\t Time:\n");
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        for (int j=0;j<32;j++){
          tot_errors[i][j] = 0;
          lprintf("%d\t %d\t %d\t %d",crateNum,i,j,times[i*32+j]);
          if (times[i*32+j] == 9999){
            lprintf(">>> Bad time measurement\n");
            tot_errors[i][j] = 3;
          }
          if (targetTime > times[i*32+j]){
            if (targetTime < 9999){
              lprintf(">>> Warning: Time less than %d nsec",targetTime);
              tot_errors[i][j] = 1;
            }
          }else if(targetTime == 9999){
            lprintf(">>> Problem measuring time for this channel\n");
            tot_errors[i][j] = 2;
          }
          lprintf("\n");
        }
      }
    }

    if (updateDB){
      lprintf("updating the database\n");
      int slot;
      for (slot=0;slot<16;slot++){
        if ((0x1<<slot) & slotMask){
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("get_ttot"));
          json_append_member(newdoc,"targettime",json_mknumber((double)targetTime));

          JsonNode *channels = json_mkarray();
          int passflag = 1;
          for (int i=0;i<32;i++){
            if (tot_errors[slot][i] == 1)
              passflag = 0;
            JsonNode *one_chan = json_mkobject();
            json_append_member(one_chan,"id",json_mknumber((double) i));
            json_append_member(one_chan,"time",json_mknumber((double) times[slot*32+i]));
            json_append_member(one_chan,"errors",json_mknumber(tot_errors[slot][i]));
            json_append_element(channels,one_chan);
          }
          json_append_member(newdoc,"channels",channels);

          json_append_member(newdoc,"pass",json_mkbool(passflag));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][slot]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,slot,newdoc);
          json_delete(newdoc); // delete the head ndoe
        }
      }
    }

  }
  catch(const char* s){
    lprintf("GetTTot: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}

int SetTTot(int crateNum, uint32_t slotMask, int targetTime, int updateDB, int finalTest, int ecal)
{
  uint16_t allrmps[16][8],allvsis[16][8],alltimes[16*32];
  int tot_errors[16][32];
  uint16_t rmp[8],vsi[8],rmpup[8],vli[8];
  uint16_t rmp_high[8],rmp_low[8];
  uint16_t chips_not_finished;
  int diff[32];
  uint32_t dac_nums[50],dac_values[50],slot_nums[50];
  int num_dacs;
  int result;

  try {

    // setup the mtc with the triggers going to the TUB
    int errors = mtc->SetupPedestals(0,60,100,0,(0x1<<crateNum) | MSK_CRATE21,MSK_CRATE21);

    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        // set default values
        for (int j=0;j<8;j++){
          rmpup[j] = RMPUP_DEFAULT;
          vsi[j] = VSI_DEFAULT;
          vli[j] = VLI_DEFAULT;
          rmp_high[j] = MAX_RMP_VALUE;
          rmp_low[j] = RMP_DEFAULT-10;
          rmp[j] = (int) (rmp_high[j] + rmp_low[j])/2;
        }

        // first check that if we make ttot as short as possible, triggers show up 
        num_dacs = 0;
        for (int j=0;j<8;j++){
          dac_nums[num_dacs] = d_rmpup[j];
          dac_values[num_dacs] = rmpup[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vli[j];
          dac_values[num_dacs] = vli[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_rmp[j];
          dac_values[num_dacs] = rmp_low[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vsi[j];
          dac_values[num_dacs] = vsi[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
        }
        xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums);

        for (int k=0;k<32;k++){
          result = CheckTTot(crateNum,i,(0x1<<k),MAX_TIME,diff);
          tot_errors[i][k] = 0;
          if (diff[k] == 1){
            lprintf("Error - Not getting TUB triggers on channel %d!\n",k);
            tot_errors[i][k] = 2;
          }
        }

        // load default values
        num_dacs = 0;
        for (int j=0;j<8;j++){
          dac_nums[num_dacs] = d_rmp[j];
          dac_values[num_dacs] = rmp[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vsi[j];
          dac_values[num_dacs] = vsi[j];
          slot_nums[num_dacs] = i;
          num_dacs++;
        }
        xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums);

        lprintf("Setting ttot for crate/board %d %d, target time %d\n",crateNum,i,targetTime);
        chips_not_finished = 0xFF;

        // loop until all ttot measurements are larger than target ttime
        while(chips_not_finished){

          // measure ttot for all chips
          result = CheckTTot(crateNum,i,0xFFFFFFFF,targetTime,diff);

          // loop over disc chips
          for (int j=0;j<8;j++){
            if ((0x1<<j) & chips_not_finished){

              // check if above or below
              if ((diff[4*j+0] > 0) && (diff[4*j+1] > 0) && (diff[4*j+2] > 0)
                  && (diff[4*j+3] > 0)){
                //lprintf("above\n");
                rmp_high[j] = rmp[j];

                // if we have narrowed it down to the first setting that works, we are done
                if ((rmp[j] - rmp_low[j]) == 1){
                  lprintf("Chip %d finished\n",j);
                  chips_not_finished &= ~(0x1<<j);
                  allrmps[i][j] = rmp[j];
                  allvsis[i][j] = vsi[j];
                }
              }else{
                //lprintf("below\n");
                rmp_low[j] = rmp[j];
                if (rmp[j] == MAX_RMP_VALUE){
                  if (vsi[j] > MIN_VSI_VALUE){
                    rmp_high[j] = MAX_RMP_VALUE;
                    rmp_low[j] = RMP_DEFAULT-10;
                    vsi[j] -= 2;
                    //lprintf("%d - vsi: %d\n",j,vsi[j]);
                  }else{
                    // out of bounds, end loop with error
                    lprintf("RMP/VSI is too big for disc chip %d! (%d %d)\n",j,rmp[j],vsi[j]);
                    lprintf("Aborting slot %d setup.\n",i);
                    tot_errors[i][j*4+0] = 1;
                    tot_errors[i][j*4+1] = 1;
                    tot_errors[i][j*4+2] = 1;
                    tot_errors[i][j*4+3] = 1;
                    for (int l=0;l<8;l++)
                      if (chips_not_finished & (0x1<<l)){
                        lprintf("Slot %d Chip %d\tRMP/VSI: %d %d <- unfinished\n",i,l,rmp[l],vsi[l]);
                        allrmps[i][l] = rmp[l];
                        allvsis[i][l] = vsi[l];
                      }
                    chips_not_finished = 0x0;
                  }
                }else if (rmp[j] == rmp_high[j]){
                  // in case it screws up and fails after it succeeded already
                  rmp_high[j]++;
                }
              }

              rmp[j] = (int) ((float) (rmp_high[j] + rmp_low[j])/2.0 + 0.5);

            } // end if this chip not finished
          } // end loop over disc chips

          // load new values
          num_dacs = 0;
          for (int j=0;j<8;j++){
            dac_nums[num_dacs] = d_rmp[j];
            dac_values[num_dacs] = rmp[j];
            slot_nums[num_dacs] = i;
            num_dacs++;
            dac_nums[num_dacs] = d_vsi[j];
            dac_values[num_dacs] = vsi[j];
            slot_nums[num_dacs] = i;
            num_dacs++;
          }
          xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums);

        } // end while chips_not_finished

        // now get the final timing measurements
        num_dacs = 0;
        for (int j=0;j<8;j++){
          dac_nums[num_dacs] = d_rmp[j];
          dac_values[num_dacs] = allrmps[i][j];
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vsi[j];
          dac_values[num_dacs] = allvsis[i][j];
          slot_nums[num_dacs] = i;
          num_dacs++;
        }
        xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums);

        result = MeasureTTot(crateNum,(0x1<<i),150,alltimes);

        lprintf("Final timing measurements:\n");
        for (int j=0;j<8;j++){
          lprintf("Chip %d (RMP/VSI %d %d) Times:\t%d\t%d\t%d\t%d\n",
              j,rmp[j],vsi[j],alltimes[i*32+j*4+0],alltimes[i*32+j*4+1],
              alltimes[i*32+j*4+2],alltimes[i*32+j*4+3]);
        }

        if (updateDB){
          lprintf("updating the database\n");
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("set_ttot"));
          json_append_member(newdoc,"targettime",json_mknumber((double)targetTime));

          JsonNode *all_chips = json_mkarray();
          int passflag = 1;
          int k;
          for (int k=0;k<8;k++){
            JsonNode *one_chip = json_mkobject();
            json_append_member(one_chip,"rmp",json_mknumber((double) allrmps[i][k]));
            json_append_member(one_chip,"vsi",json_mknumber((double) allvsis[i][k]));
            json_append_member(one_chip,"rmpup",json_mknumber((double) rmpup[k]));
            json_append_member(one_chip,"vli",json_mknumber((double) vli[k]));

            JsonNode *all_chans = json_mkarray();
            for (int j=0;j<4;j++){
              JsonNode *one_chan = json_mkobject();
              if (tot_errors[i][k*4+j]> 0)
                passflag = 0;
              json_append_member(one_chan,"id",json_mknumber((double) k*4+j));
              json_append_member(one_chan,"time",json_mknumber((double) alltimes[i*32+k*4+j]));
              json_append_member(one_chan,"errors",json_mknumber(tot_errors[i][k*4+j]));
              json_append_element(all_chans,one_chan);
            }
            json_append_member(one_chip,"channels",all_chans);

            json_append_element(all_chips,one_chip);
          }
          json_append_member(newdoc,"chips",all_chips);

          json_append_member(newdoc,"pass",json_mkbool(passflag));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // head node needs deleting
        }
      } // if in slot mask
    } // end loop over slots

    lprintf("Set ttot complete\n");

  }
  catch(const char* s){
    lprintf("SetTTot: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}


int MeasureTTot(int crate, uint32_t slot_mask, int start_time, uint16_t *disc_times)
{
  int increment = 1;
  int time;
  uint32_t chan_done_mask;
  float real_delay;
  uint32_t init[32],fin[32];
  uint32_t temp[8][32];

  for (int i=0;i<16;i++){
    if ((0x1<<i) & slot_mask){
      int result = xl3s[crate]->SetCratePedestals(0x1<<i,0xFFFFFFFF);
      chan_done_mask = 0x0;
      time = start_time;
      while (chan_done_mask != 0xFFFFFFFF){
        // set up gt delay
        real_delay = mtc->SetGTDelay((float) time);
        while ((real_delay > (float) time) || ((real_delay + (float) increment) < (float) time)){
          lprintf("got %f instead of %f, trying again\n",real_delay,(float) time);
          real_delay = mtc->SetGTDelay((float) time);
        }
        // get the cmos count before sending pulses
        result = xl3s[crate]->GetCmosTotalCount(0x1<<i,temp);
        for (int j=0;j<32;j++)
          init[j] = temp[0][j];
        // send some pulses
        mtc->MultiSoftGT(NUM_PEDS);
        //now read out the count again to get the rate
        result = xl3s[crate]->GetCmosTotalCount(0x1<<i,temp);
        for (int j=0;j<32;j++)
          fin[j] = temp[0][j];
        for (int j=0;j<32;j++){
          fin[j] -= init[j];
          //lprintf("for %d at time %d, got %d of %d\n",j,time,fin[j],2*NUM_PEDS);
          // check if we got all the pedestals from the TUB too
          if ((fin[j] >= 2*NUM_PEDS) && ((0x1<<j) & ~chan_done_mask)){
            chan_done_mask |= (0x1<<j); 
            disc_times[i*32+j] = (int)real_delay+TUB_DELAY;
          }
        }
        if (chan_done_mask == 0xFFFFFFFF)
          break;
        if (time >= MAX_TIME){
          for (int k=0;k<32;k++){
            if ((0x1<<k) & ~chan_done_mask)
              disc_times[i*32+k] = time+TUB_DELAY;
            chan_done_mask = 0xFFFFFFFF;
          }
        }else{
          if (((int) (real_delay + 0.5) + increment) > time)
            time = (int) (real_delay + 0.5) + increment;
          if (time > MAX_TIME)
            time = MAX_TIME;
        }
      } // for time<=MAX_TIME

      // now that we got our times, check each channel one by one to ensure it was
      // working on its own
      for (int j=0;j<32;j++){
        result = xl3s[crate]->SetCratePedestals(0x1<<i,0x1<<j);
        // if it worked before at time-tub_delay, it should work for time-tub_delay+50
        real_delay = mtc->SetGTDelay((float) disc_times[i*32+j]-TUB_DELAY+50);
        while (real_delay < ((float) disc_times[i*32+j] - TUB_DELAY + 50 - 5))
        {
          lprintf("2 - got %f instead of %f, trying again\n",real_delay,(float) disc_times[i*32+j] - TUB_DELAY + 50);
          real_delay = mtc->SetGTDelay((float) disc_times[i*32+j]-TUB_DELAY+50);
        }

        result = xl3s[crate]->GetCmosTotalCount(0x1<<i,temp);
        init[j] = temp[0][j];
        mtc->MultiSoftGT(NUM_PEDS);
        result = xl3s[crate]->GetCmosTotalCount(0x1<<i,temp);
        fin[j] = temp[0][j];
        fin[j] -= init[j];
        if (fin[j] < 2*NUM_PEDS){
          // we didn't get the peds without the other channels enabled
          lprintf("Error channel %d - pedestals went away after other channels turned off!\n",j);
          disc_times[i*32+j] = 9999;
        }
      }

    } // end if slot mask
  } // end loop over slots
  return 0;
}

int CheckTTot(int crate, int slot_num, uint32_t chan_mask, int goal_time, int *diff)
{
  float real_delay;
  uint32_t init[32],fin[32];
  uint32_t temp[8][32];

  // initialize array
  for (int i=0;i<32;i++)
    diff[i] = 0;

  int result = xl3s[crate]->SetCratePedestals(0x1<<slot_num,chan_mask);

  // measure it twice to make sure we are good
  for (int i=0;i<2;i++){
    real_delay = mtc->SetGTDelay((float) goal_time - TUB_DELAY);
    while (real_delay < ((float) goal_time - TUB_DELAY - 5))
    {
      lprintf("3 - got %f instead of %f, trying again\n",real_delay,(float)goal_time - TUB_DELAY);
      real_delay = mtc->SetGTDelay((float) goal_time - TUB_DELAY);
    }

    // get the cmos count before sending pulses
    result = xl3s[crate]->GetCmosTotalCount(0x1<<slot_num,temp);
    for (int j=0;j<32;j++)
      init[j] = temp[0][j];
    // send some pulses
    mtc->MultiSoftGT(NUM_PEDS);
    // now read out the count again to get the rate
    result = xl3s[crate]->GetCmosTotalCount(0x1<<slot_num,temp);
    for (int j=0;j<32;j++)
      fin[j] = temp[0][j];
    for (int k=0;k<32;k++){
      fin[k] -= init[k];
      // check if we got all the peds from the TUB too
      if (fin[k] < 2*NUM_PEDS){
        // we didnt get all the peds, so ttot is longer than our target time
        if (i==0)
          diff[k] = 1;
      }else{
        //lprintf("%d was short. Got %d out of %d (%d before, %d after)\n",k,fin[k],2*NUM_PEDS,init[k],fin[k]+init[k]);
        // if its shorter either time, flag it as too short
        diff[k] = 0;
      }
    }
  }
  return 0;
}
