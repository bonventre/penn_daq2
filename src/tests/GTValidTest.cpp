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
              printf("no\n");
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
        // now lets see which channel has the max gtvalid for each tac at some
        // default isetm
        error+= xl3s[crateNum]->LoadsDac(d_isetm[0],ISETM_FIND_MAX,i);
        error+= xl3s[crateNum]->LoadsDac(d_isetm[1],ISETM_FIND_MAX,i);
        if (twiddleOn){
          error+= xl3s[crateNum]->LoadsDac(d_iseta[0],ISETA,i);
          error+= xl3s[crateNum]->LoadsDac(d_iseta[1],ISETA,i);
          for (int j=0;j<32;j++)
            tacbits[j] = 0x77;
          error+= xl3s[crateNum]->LoadTacbits(i,tacbits);
        }

        for (int wt=0;wt<2;wt++){
          lprintf("\nMeasuring GTVALID for crate %d, slot %d, TAC %d\n",
              crateNum,i,wt);

          // loop over channel to measure inital GTVALID and find channel with max
          for (int j=0;j<32;j++){
            if ((0x1<<j) & channelMask){
              error+= xl3s[crateNum]->LoadsDac(d_isetm[0],ISETM_FIND_MAX,i);
              error+= xl3s[crateNum]->LoadsDac(d_isetm[1],ISETM_FIND_MAX,i);
              xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
              gtvalid_first[wt][j] = MeasureGTValid(crateNum,i,wt,max_gtvalid[j],max_isetm[j]);
            } // end if chan mask
          } // end loop over channels
          // find maximum gtvalid time
          gmax[wt] = 0.0;
          cmax[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtvalid_first[wt][j] > gmax[wt]){
                gmax[wt] = gtvalid_first[wt][j];
                cmax[wt] = j;
              }

          // find minimum gtvalid time
          gmin[wt] = 9999.0;
          cmin[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtvalid_first[wt][j] < gmin[wt]){
                gmin[wt] = gtvalid_first[wt][j];
                cmin[wt] = j;
              }


        } // end loop over tacs
        lprintf("GTVALID initial results, time in ns:\n");
        lprintf("---------------------------------------\n");
        lprintf("Crate Slot Chan GTDELAY 0/1:\n");
        for (int j=0;j<32;j++){
          if ((0x1<<j) & channelMask)
            lprintf("%02d %02d %02d %4.1f %4.1f\n",crateNum,i,j,gtvalid_first[0][j],gtvalid_first[1][j]);
        }
        lprintf("TAC 0 max at chan %02d: %4.1f\n",cmax[0],gmax[0]);
        lprintf("TAC 1 max at chan %02d: %4.1f\n",cmax[1],gmax[1]);
        lprintf("TAC 0 min at chan %02d: %4.1f\n",cmin[0],gmin[0]);
        lprintf("TAC 1 min at chan %02d: %4.1f\n",cmin[1],gmin[1]);


        // now we'll find the right dac values to get all gtvalids under gtCutoff
        for (int wt=0;wt<2;wt++){
          lprintf("Finding ISETM values for crate %d, slot %d TAC %d\n",
              crateNum,i,wt);
          if (gmax[wt] > gtCutoff)
            isetm[wt] = ISETM_FIND_MAX;
          else
            isetm[wt] = ISETM;

          xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,0x1<<cmax[wt],&result);
          error = xl3s[crateNum]->LoadsDac(d_isetm[0],max_isetm[cmax[0]],i);
          error = xl3s[crateNum]->LoadsDac(d_isetm[1],max_isetm[cmax[1]],i);

          while (isetm[wt] < 255){
            // load a new dac value
            error = xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm[wt],i);
            if (error){
              lprintf("Error loading Dacs. Exiting\n");
              return -1;
            }

            lprintf(".");
            fflush(stdout);
            if (IsGTValidLonger(crateNum,i,gtCutoff)){
              isetm[wt]++;
            }else{
              break;
            }
          }
          lprintf("\n");
        } // end loop over tacs
        lprintf("\nFound ISETM value, checking new maximum\n");

        // we've set ISETM for each tac, lets check if the channel with the
        // maximum gtvalid has changed

        for (int wt=0;wt<2;wt++){
          lprintf("\nMeasuring GTVALID for crate %d, slot %d, TAC %d\n",
              crateNum,i,wt);

          // loop over channel to measure inital GTVALID and find channel with max
          for (int j=0;j<32;j++){
            if ((0x1<<j) & channelMask){
              error+= xl3s[crateNum]->LoadsDac(d_isetm[0],isetm[0],i);
              error+= xl3s[crateNum]->LoadsDac(d_isetm[1],isetm[1],i);
              xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,1<<j,&result);
              gtvalid_second[wt][j] = MeasureGTValid(crateNum,i,wt,max_gtvalid[j],max_isetm[j]);
            } // end if chan mask
          } // end loop over channels
          // find maximum gtvalid time
          int max_changed = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtvalid_second[wt][j] > gmax[wt]){
                gmax[wt] = gtvalid_second[wt][j];
                cmax[wt] = j;
                max_changed = 1;
              }

          if (max_changed){
            lprintf("Warning, second chan_max not same as first.\n");
            lprintf("Finding ISETM values for crate %d, slot %d TAC %d\n",
                crateNum,i,wt);

            xl3s[crateNum]->RW(PED_ENABLE_R + FEC_SEL*i + WRITE_REG,0x1<<cmax[wt],&result);
            error = xl3s[crateNum]->LoadsDac(d_isetm[wt],max_isetm[cmax[wt]],i);
            error = xl3s[crateNum]->LoadsDac(d_isetm[wt],max_isetm[cmax[wt]],i);

            while (isetm[wt] < 255){
              // load a new dac value
              error = xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm[wt],i);
              if (error){
                lprintf("Error loading Dacs. Exiting\n");
                return -1;
              }

              lprintf(".");
              fflush(stdout);
              if (IsGTValidLonger(crateNum,i,gtCutoff)){
                isetm[wt]++;
              }else{
                break;
              }
            }
            lprintf("\n");

          }



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
        lprintf("\nCrate %d Slot %d - GTVALID FINAL results, time in ns:\n",crateNum,i);
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
            lprintf("%d 0x%02x %f %f",
                j,tacbits[j]*16 + tacbits[j],
                gtvalid_final[0][j],gtvalid_final[1][j]);
            if (isetm[0] == ISETM || isetm[1] == ISETM)
              lprintf(">>> Warning: isetm not adjusted\n");
            else
              lprintf("\n");
          }
        }

        lprintf(">>> Maximum TAC0 GTValid - Chan %d: %f\n",
            cmax[0],gmax[0]);
        lprintf(">>> Minimum TAC0 GTValid - Chan %d: %f\n",
            cmin[0],gmin[0]);
        lprintf(">>> Maximum TAC1 GTValid - Chan %d: %f\n",
            cmax[1],gmax[1]);
        lprintf(">>> Minimum TAC1 GTValid - Chan %d: %f\n",
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



      } // end in slotmask
    } // end loop over slots
  } // end try
  catch(const char* s){
    lprintf("GTValidTest: %s\n",s);
  }
  return 0;
}
/*
// ok so now isetm has been tweaked just enough
// so that the max channel is at gtcutoff
// we now use the twiddle bits to get the other
// channels close too
if (twiddleOn){
for (int j=0;j<32;j++)
gtflag[wt][j] = 0;
gtflag[wt][cmax[wt]] = 1; // max channel is already done
for (int j=0;j<32;j++){
if (gtchan[j] < 0)
gtflag[wt][cmax[wt]] = 1; // skip any buggy channels
}
done = 0;
while (!done){
// loop over all channels not yet finished with
// and measure current gtdelay
for (int j=0;j<32;j++){
if (((0x1<<j) & channelMask) && gtflag[wt][j] == 0){
xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<j,&result);
error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_new[0],isetm_new[1]);
if (error != 0){
lprintf("Error getting gtdelay at slot %d, channel %d\n",i,j);
return -1;
}
gtchan_set[wt][j] = gt_temp;
}
}
done = 1;
// now successively turn off twiddle bits
lprintf("\n");
for (int j=0;j<32;j++){
if ((0x1<<j) & channelMask){
if ((gtchan_set[wt][j] <= gtCutoff) && (gtflag[wt][j] == 0) &&
(tacbits_new[wt][j] != 0x0)){
tacbits_new[wt][j]-=0x1; // decrement twiddle by 1
done = 0;
lprintf("Channel %d, %f, tacbits at %01x\n",j,gtchan_set[wt][j],tacbits_new[wt][j]);
}else if ((gtchan_set[wt][j] > gtCutoff) && (gtflag[wt][j] == 0)){
tacbits_new[wt][j] += 0x1; // go up just one
if (tacbits_new[wt][j] > 0x7)
tacbits_new[wt][j] = 0x7; //max
gtflag[wt][j] = 1; // this channel is as close to gt_cutoff as possible
lprintf("Channel %d ok\n",j);
}
}
}
// now build and load the new tacbits
for (int j=0;j<32;j++)
tacbits[j] = tacbits_new[1][j]*16 + tacbits_new[0][j];
packet.header.packetType = LOAD_TACBITS_ID;
LoadTacBitsArgs *args = (LoadTacBitsArgs *) packet.payload;
args->crateNum = crateNum;
args->selectReg = select_reg;
for (int j=0;j<32;j++)
args->tacBits[j] = tacbits[j];
SwapLongBlock(packet.payload,2);
SwapShortBlock(packet.payload+8,32);
xl3s[crateNum]->SendCommand(&packet);
lprintf("TAC bits loaded\n");

} // end while not done
} // end if do_twiddle

// we are done, save the setup
isetm_save[wt] = isetm_new[wt];
if (twiddleOn)
for (int j=0;j<32;j++)
tacbits_save[wt][j] = tacbits_new[wt][j];

// remeasure gtvalid
for (int j=0;j<32;j++)
gtchan_set[wt][j] = 9999;
for (int j=0;j<32;j++){
  if ((0x1<<j) & channelMask){
    xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<j,&result);
    error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_new[0],isetm_new[1]);
    if (error != 0){
      lprintf("Error getting gtdelay at slot %d, channel %d\n",i,j);
      return -1;
    }
    gtchan_set[wt][j] = gt_temp;
  }
}
// find maximum
gt_max_set[wt] = 0.0;
chan_max_set[wt] = 0;
  for (int j=0;j<32;j++)
if ((0x1<<j) & channelMask)
  if (gtchan_set[wt][j] > gt_max_set[wt]){
    gt_max_set[wt] = gtchan_set[wt][j];
    chan_max_set[wt] = j;
  }
// find minimum
gt_min_set[wt] = 9999.0;
chan_min_set[wt] = 0;
  for (int j=0;j<32;j++)
if ((0x1<<j) & channelMask)
  if (gtchan_set[wt][j] < gt_min_set[wt]){
    gt_min_set[wt] = gtchan_set[wt][j];
    chan_min_set[wt] = j;
  }

} // end if gt_cutoff != 0

} // end loop over tacs

// reset pedestals
xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x0,&result);

// print out
lprintf("\nCrate %d Slot %d - GTVALID FINAL results, time in ns:\n",crateNum,i);
lprintf("--------------------------------------------------------\n");
if (!twiddleOn)
  lprintf(" >>> ISETA0/1 = 0, no TAC twiddle bits set\n");
  lprintf("set up: VMAX: %hu, TACREF: %hu, ",VMAX,TACREF);
if (twiddleOn)
  lprintf("ISETA: %hu\n",ISETA);
  else
  lprintf("ISETA: %hu\n",ISETA_NO_TWIDDLE);
  lprintf("Found ISETM0: %d, ISETM1: %d\n",isetm_save[0],isetm_save[1]);
  lprintf("Chan Tacbits GTValid 0/1:\n");
  for (int j=0;j<32;j++){
    if ((0x1<<j) & channelMask){
      lprintf("%d 0x%02x %f %f",
          j,tacbits_save[1][j]*16 + tacbits_save[0][j],
          gtchan_set[0][j],gtchan_set[1][j]);
      if (isetm_save[0] == ISETM || isetm_save[1] == ISETM)
        lprintf(">>> Warning: isetm not adjusted\n");
      else
        lprintf("\n");
    }
  }

lprintf(">>> Maximum TAC0 GTValid - Chan %d: %f\n",
    chan_max_set[0],gt_max_set[0]);
lprintf(">>> Minimum TAC0 GTValid - Chan %d: %f\n",
    chan_min_set[0],gt_min_set[0]);
lprintf(">>> Maximum TAC1 GTValid - Chan %d: %f\n",
    chan_max_set[1],gt_max_set[1]);
lprintf(">>> Minimum TAC1 GTValid - Chan %d: %f\n",
    chan_min_set[1],gt_min_set[1]);

if (abs(isetm_save[1] - isetm_save[0]) > 10)
  slot_errors |= 0x1;
  for (int j=0;j<32;j++){
    if ((gtchan_set[0][j] < 0) || gtchan_set[1][j] < 0)
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
  json_append_element(isetm_new,json_mknumber((double)isetm_save[0]));
  json_append_element(isetm_new,json_mknumber((double)isetm_save[1]));
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
    json_append_member(one_chan,"tac_shift",json_mknumber((double) (tacbits_save[1][j]*16+tacbits_save[0][1])));
    json_append_member(one_chan,"gtvalid0",json_mknumber((double) (gtchan_set[0][j])));
    json_append_member(one_chan,"gtvalid1",json_mknumber((double) (gtchan_set[1][j])));
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

} // end if slot mask
} // end loop over slot

}
catch(const char* s){
  lprintf("GTValidTest: %s\n",s);
}

lprintf("****************************************\n");
return 0;
}
*/

// returns 1 if gtvalid is longer than time, 0 otherwise.
// if gtvalid is longer should get hits generated from all the pedestals
int IsGTValidLonger(int crateNum, int slotNum, float time)
{
  uint32_t result;
  // reset fifo
  xl3s[crateNum]->RW(GENERAL_CSR_R + FEC_SEL*slotNum + WRITE_REG,0x2,&result);
  xl3s[crateNum]->RW(GENERAL_CSR_R + FEC_SEL*slotNum + WRITE_REG,0x0,&result);
  mtc->SetGTDelay(time+GTPED_DELAY+TDELAY_EXTRA); 

  mtc->MultiSoftGT(NGTVALID);
  usleep(500);

  xl3s[crateNum]->RW(FIFO_WRITE_PTR_R + FEC_SEL*slotNum + READ_REG,0x0,&result);
  int num_read = (result & 0x000FFFFF)/3;
  if (num_read < (NGTVALID)*0.75)
    return 0;
  else
    return 1;
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

    if (upper_limit - lower_limit <= 25){
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


