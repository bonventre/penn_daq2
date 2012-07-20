#include <stdlib.h>
#include <math.h>

#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "DacNumber.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "GTValidTest.h"

int GTValidTest(int crateNum, uint32_t slotMask, uint32_t channelMask, float gtCutoff, int twiddleOn, int updateDB, int finalTest, int ecal)
{
  lprintf("*** Starting GT Valid Test *************\n");

  uint32_t result;
  int error;
  int slot_errors;
  int chan_errors[32];

  uint16_t tacbits[32];
  uint16_t max_isetm[32],isetm[2];
  float max_gtvalid[32], gtvalid_first[2][32], gtvalid_second[2][32], gtvalid_final[2][32];
  float gmax[2],gmin[2];
  int cmax[2],cmin[2];


  try{

    // setup crate
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        uint32_t select_reg = FEC_SEL*i;
        // disable pedestals
        xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result);
        // reset fifo
        xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + READ_REG,0x0,&result);
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,
            result | (crateNum << FEC_CSR_CRATE_OFFSET) | 0x6,&result);
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,
            (crateNum << FEC_CSR_CRATE_OFFSET),&result);
        xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result);
      }
    }
    xl3s[crateNum]->DeselectFECs();

    if (mtc->SetupPedestals(0,DEFAULT_PED_WIDTH,10,0,(0x1<<crateNum),(0x1<<crateNum))){
      lprintf("Error setting up mtc. Exiting\n");
      return -1;
    }

    // loop over slot
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        uint32_t select_reg = FEC_SEL*i;

        slot_errors = 0;
        for (int j=0;j<32;j++){
          chan_errors[j] = 0;
        }
        error = xl3s[crateNum]->LoadsDac(d_vmax,VMAX,i);
        error+= xl3s[crateNum]->LoadsDac(d_tacref,TACREF,i);

        // We first need to find the max gtvalid per channel, and find the
        // ISETM settings per channel for max gtvalid 
        lprintf("Finding max possible gtvalids\n");
        
        // turn off twiddle bits
        error+= xl3s[crateNum]->LoadsDac(d_iseta[0],ISETA_NO_TWIDDLE,i);
        error+= xl3s[crateNum]->LoadsDac(d_iseta[1],ISETA_NO_TWIDDLE,i);
        for (int j=0;j<32;j++)
          tacbits[j] = 0x00;
        error+= xl3s[crateNum]->LoadTacbits(i,tacbits);
        if (error){
          lprintf("Error setting up TAC voltages. Exiting\n");
          return -1;
        }

        // loop over channels
        for (int j=0;j<32;j++){
          if ((0x1<<j) & channelMask){
            xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
            max_gtvalid[j] = 0;
            // first try with the default ISETM
            error+= xl3s[crateNum]->LoadsDac(d_isetm[0],ISETM_MAX_GTVALID,i);
            error+= xl3s[crateNum]->LoadsDac(d_isetm[1],ISETM_MAX_GTVALID,i);
            if (IsGTValidLonger(crateNum,i,GTMAX)){
              max_gtvalid[j] = GTMAX;
              max_isetm[j] = ISETM_MAX_GTVALID;
            }else{
              // scan to see if any ISETM value puts this channel over GTMAX
              int done = 0;
              for (int k=0;k<8;k++){
                float max_time = 1000.0-100.0*k;
                for (int l=0;l<50;l++){
                  uint32_t isetm_temp = l*5;
                  error+= xl3s[crateNum]->LoadsDac(d_isetm[0],isetm_temp,i);
                  error+= xl3s[crateNum]->LoadsDac(d_isetm[1],isetm_temp,i);
                  if (IsGTValidLonger(crateNum,i,max_time)){
                    max_gtvalid[j] = max_time;
                    max_isetm[j] = isetm_temp;
                    done = 1;
                    break;
                  }
                }
                if (done)
                  break;
              }
              // if the max gtvalid time is too small, fail this channel
              if (max_gtvalid[j] == 0)
                chan_errors[j] = 1; 
            }
          } // end channel mask
        } // end loop over channels




        // ok we now know what the max gtvalid is for each channel and what
        // isetm value will get us it
        // now we increment isetm until every channels gtvalid is shorter than
        // gtcutoff
        for (int wt=0;wt<2;wt++){
          lprintf("Finding ISETM values for crate %d, slot %d TAC %d\n",
              crateNum,i,wt);
          int ot = (wt+1)%2;
          isetm[wt] = ISETM_MIN;
          for (int j=0;j<32;j++){
            lprintf(".");
            fflush(stdout);
            xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
            error+= xl3s[crateNum]->LoadsDac(d_isetm[ot],max_isetm[j],i);
            while (isetm[wt] < 255){
              error+= xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm[wt],i);
              if (IsGTValidLonger(crateNum,i,gtCutoff)){
                isetm[wt]++;
              }else{
                break;
              }
            }
          }
          for (int j=0;j<32;j++){
            lprintf(".");
            fflush(stdout);
            xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
            error+= xl3s[crateNum]->LoadsDac(d_isetm[ot],max_isetm[j],i);
            while (isetm[wt] < 255){
              error+= xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm[wt],i);
              if (IsGTValidLonger(crateNum,i,gtCutoff)){
                isetm[wt]++;
                printf("incremented again!\n");
              }else{
                break;
              }
            }
          }

          printf("\n");
        } // end loop over tacs


        // we are done getting our dac values. lets measure and display the final gtvalids
        for (int wt=0;wt<2;wt++){
          lprintf("\nMeasuring GTVALID for crate %d, slot %d, TAC %d\n",
              crateNum,i,wt);

          // loop over channel to measure inital GTVALID and find channel with max
          for (int j=0;j<32;j++){
            if ((0x1<<j) & channelMask){
              error+= xl3s[crateNum]->LoadsDac(d_isetm[0],isetm[0],i);
              error+= xl3s[crateNum]->LoadsDac(d_isetm[1],isetm[1],i);
              xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
              gtvalid_final[wt][j] = MeasureGTValid(crateNum,i,wt,max_gtvalid[j],max_isetm[j]);
            } // end if chan mask
          } // end loop over channels
          // find maximum gtvalid time
          gmax[wt] = 0.0;
          cmax[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtvalid_final[wt][j] > gmax[wt]){
                gmax[wt] = gtvalid_final[wt][j];
                cmax[wt] = j;
              }

          // find minimum gtvalid time
          gmin[wt] = 9999.0;
          cmin[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtvalid_final[wt][j] < gmin[wt]){
                gmin[wt] = gtvalid_final[wt][j];
                cmin[wt] = j;
              }


        } // end loop over tacs

        // print out
        lprintf("\n--------------------------------------------------------\n");
        lprintf("Crate %d Slot %d - GTVALID FINAL results, time in ns:\n",crateNum,i);
        lprintf("--------------------------------------------------------\n");
        if (!twiddleOn)
          lprintf(" >>> ISETA0/1 = 0, no TAC twiddle bits set\n");
        lprintf("set up: VMAX: %hu, TACREF: %hu, ",VMAX,TACREF);
        if (twiddleOn)
          lprintf("ISETA: %hu\n",ISETA);
        else
          lprintf("ISETA: %hu\n",ISETA_NO_TWIDDLE);
        lprintf("Found ISETM0: %d, ISETM1: %d\n",isetm[0],isetm[1]);
        lprintf("Chan Tacbits GTValid 0/1:\n");
        for (int j=0;j<32;j++){
          if ((0x1<<j) & channelMask){
            lprintf("%02d 0x%02x %4.1f %4.1f",
                j,tacbits[j],
                gtvalid_final[0][j],gtvalid_final[1][j]);
            if (isetm[0] == ISETM_MIN || isetm[1] == ISETM_MIN)
              lprintf(">>> Warning: isetm not adjusted\n");
            else
              lprintf("\n");
          }
        }

        lprintf(">>> Maximum TAC0 GTValid - Chan %02d: %4.1f\n",
            cmax[0],gmax[0]);
        lprintf(">>> Minimum TAC0 GTValid - Chan %02d: %4.1f\n",
            cmin[0],gmin[0]);
        lprintf(">>> Maximum TAC1 GTValid - Chan %02d: %4.1f\n",
            cmax[1],gmax[1]);
        lprintf(">>> Minimum TAC1 GTValid - Chan %02d: %4.1f\n",
            cmin[1],gmin[1]);

        if (abs(isetm[1] - isetm[0]) > 10)
          slot_errors |= 0x1;
        for (int j=0;j<32;j++){
          if ((gtvalid_final[0][j] < 0) || gtvalid_final[1][j] < 0)
            chan_errors[j] = 1;
        }


        //store in DB
        if (updateDB){
          lprintf("updating the database\n");
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("cmos_m_gtvalid"));

          json_append_member(newdoc,"vmax",json_mknumber((double)VMAX));
          json_append_member(newdoc,"tacref",json_mknumber((double)TACREF));

          JsonNode* isetm_new = json_mkarray();
          JsonNode* iseta_new = json_mkarray();
          json_append_element(isetm_new,json_mknumber((double)isetm[0]));
          json_append_element(isetm_new,json_mknumber((double)isetm[1]));
          if (twiddleOn){
            json_append_element(iseta_new,json_mknumber((double)ISETA));
            json_append_element(iseta_new,json_mknumber((double)ISETA));
          }else{
            json_append_element(iseta_new,json_mknumber((double)ISETA_NO_TWIDDLE));
            json_append_element(iseta_new,json_mknumber((double)ISETA_NO_TWIDDLE));
          }
          json_append_member(newdoc,"isetm",isetm_new);
          json_append_member(newdoc,"iseta",iseta_new);

          JsonNode* channels = json_mkarray();
          for (int j=0;j<32;j++){
            JsonNode *one_chan = json_mkobject();
            json_append_member(one_chan,"id",json_mknumber((double) j));
            json_append_member(one_chan,"tac_shift",json_mknumber((double) (tacbits[j])));
            json_append_member(one_chan,"gtvalid0",json_mknumber((double) (gtvalid_final[0][j])));
            json_append_member(one_chan,"gtvalid1",json_mknumber((double) (gtvalid_final[1][j])));
            json_append_member(one_chan,"errors",json_mkbool(chan_errors[j]));
            if (chan_errors[j])
              slot_errors |= 0x2;
            json_append_element(channels,one_chan);
          }
          json_append_member(newdoc,"channels",channels);

          json_append_member(newdoc,"pass",json_mkbool(!(slot_errors)));
          json_append_member(newdoc,"slot_errors",json_mknumber(slot_errors));
          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // only delete the head
        }
        lprintf("******************************************\n");



      } // end in slotmask
    } // end loop over slots
  } // end try
  catch(const char* s){
    lprintf("GTValidTest: %s\n",s);
  }
  return 0;
}

// returns 1 if gtvalid is longer than time, 0 otherwise.
// if gtvalid is longer should get hits generated from all the pedestals
int IsGTValidLonger(int crateNum, int slotNum, float time)
{
  usleep(5000);
  uint32_t result;
  // reset fifo
  xl3s[crateNum]->RW(GENERAL_CSR_R + FEC_SEL*slotNum + WRITE_REG,0x2,&result);
  xl3s[crateNum]->RW(GENERAL_CSR_R + FEC_SEL*slotNum + WRITE_REG,0x0,&result);
  mtc->SetGTDelay(time+GTPED_DELAY+TDELAY_EXTRA); 

  mtc->MultiSoftGT(NGTVALID);
  usleep(5000);

  xl3s[crateNum]->RW(FIFO_WRITE_PTR_R + FEC_SEL*slotNum + READ_REG,0x0,&result);
  int num_read = (result & 0x000FFFFF)/3;
  if (num_read < (NGTVALID)*0.75){
    return 0;
  }else{
    return 1;
  }
}

float MeasureGTValid(int crateNum, int slotNum, int tac, float max_gtvalid, uint32_t max_isetm)
{
  lprintf(".");
  fflush(stdout);

  uint32_t result;
  float upper_limit = max_gtvalid;
  float lower_limit = GTMIN;
  float current_delay;
  uint32_t select_reg = FEC_SEL*slotNum;

  // set unmeasured TAC gtvalid to maximum so the tac we are looking at will fail first
  int othertac = (tac+1)%2;
  int error = xl3s[crateNum]->LoadsDac(d_isetm[othertac],max_isetm,slotNum);

  while (1){
    // binary search for gtvalid
    current_delay = (upper_limit - lower_limit)/2.0 + lower_limit;
    if (IsGTValidLonger(crateNum,slotNum,current_delay))
      lower_limit = current_delay;
    else
      upper_limit = current_delay;

    if (upper_limit - lower_limit <= 1){
      break;
    }
  }


  // check if we actually found a good value or
  // if we just kept going to to the upper bound
  if (upper_limit == max_gtvalid){
    return -2;
  }else{
    // ok we know that lower limit is within the window, upper limit is outside
    // lets make sure its the right TAC failing by making the window longer and
    // seeing if the events show back up
    error = xl3s[crateNum]->LoadsDac(d_isetm[tac],max_isetm,slotNum);

    if (IsGTValidLonger(crateNum,slotNum,upper_limit)){
      return upper_limit;
    }else{
      lprintf("Uh oh, still not all the events! wrong TAC failing\n");
      return -1;
    }
  }
}


