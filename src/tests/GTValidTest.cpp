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
  printf("*** Starting GT Valid Test *************\n");

  uint32_t result;
  int error;
  XL3Packet packet;

  uint16_t isetm_new[2], tacbits[32], tacbits_new[2][32],gtflag[2][32];
  uint16_t isetm_save[2], tacbits_save[2][32], isetm_start[2];
  uint32_t nfifo, nget;
  uint16_t chan_max_set[2], chan_min_set[2], cmax[2], cmin[2];
  float gtchan[32], gtchan_set[2][32];
  float gtdelay, gtdelta, gtstart, corr_gtdelta, gt_max;
  float gt_temp, gt_max_sec, gt_min, gt_other;
  float gt_max_set[2], gt_min_set[2], gt_start[2][32];
  float ratio, best[32], gmax[2], gmin[2];
  uint16_t chan_max, chan_max_sec, chan_min;
  uint16_t ncrates;
  int done;
  uint16_t slot_errors;
  int chan_errors[32];

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
      printf("Error setting up mtc. Exiting\n");
      return -1;
    }

    // loop over slot
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        uint32_t select_reg = FEC_SEL*i;

        slot_errors = 0;
        for (int j=0;j<32;j++)
          chan_errors[j] = 0;

        // select which tac we are working on
        for (int wt=0;wt<2;wt++){

          // only set dacs if request setting gtvalid
          if (gtCutoff != 0){
            // set tac reference voltages
            error = xl3s[crateNum]->LoadsDac(d_vmax,VMAX,i);
            error+= xl3s[crateNum]->LoadsDac(d_tacref,TACREF,i);
            error+= xl3s[crateNum]->LoadsDac(d_isetm[0],ISETM_START,i);
            error+= xl3s[crateNum]->LoadsDac(d_isetm[1],ISETM_START,i);

            // enable TAC secondary current source and
            // adjust bits for all channels. The reason for
            // this is so we can turn bits off to shorten the
            // TAC time
            if (twiddleOn){
              error+= xl3s[crateNum]->LoadsDac(d_iseta[0],ISETA,i);
              error+= xl3s[crateNum]->LoadsDac(d_iseta[1],ISETA,i);
            }else{
              error+= xl3s[crateNum]->LoadsDac(d_iseta[0],ISETA_NO_TWIDDLE,i);
              error+= xl3s[crateNum]->LoadsDac(d_iseta[1],ISETA_NO_TWIDDLE,i);
            }
            if (error){
              printf("Error setting up TAC voltages. Exiting\n");
              return -1;
            }
            printf("Dacs loaded\n");

            // load cmos shift register to enable twiddle bits
            packet.header.packetType = LOAD_TACBITS_ID;
            LoadTacBitsArgs *args = (LoadTacBitsArgs *) packet.payload;
            args->crateNum = crateNum;
            args->selectReg = select_reg;
            for (int j=0;j<32;j++){
              if (twiddleOn)
                tacbits[j] = 0x77;
              else
                tacbits[j] = 0x00;
              args->tacBits[j] = tacbits[j];
            }
            SwapLongBlock(packet.payload,2);
            SwapShortBlock(packet.payload+8,32);
            xl3s[crateNum]->SendCommand(&packet);
            printf("TAC bits loaded\n");
          }

          // some other initialization
          isetm_new[0] = ISETM;
          isetm_new[1] = ISETM;
          isetm_start[0] = ISETM_START;
          isetm_start[1] = ISETM_START;
          for (int k=0;k<32;k++){
            if (twiddleOn){
              // enable all
              tacbits_new[0][k] = 0x7;
              tacbits_new[1][k] = 0x7;
            }else{
              // disable all
              tacbits_new[0][k] = 0x0;
              tacbits_new[0][k] = 0x0;
            }
          }

          printf("Measuring GTVALID for crate %d, slot %d, TAC %d\n",
              crateNum,i,wt);

          // loop over channel to measure inital GTVALID
          for (int j=0;j<32;j++){
            if ((0x1<<j) & channelMask){
              // set pedestal enable for this channel
              xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,1<<j,&result);
              error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_start[0],isetm_start[1]);
              gtchan[j] = gt_temp;
              gt_start[wt][j] = gtchan[j];
              if (error != 0){
                printf("Error getting gtdelay at slot %d, channel %d\n",i,j);
                return -1;
              }
            } // end if chan mask
          } // end loop over channels
          printf("\nMeasured initial GTValids\n");

          // find maximum gtvalid time
          gmax[wt] = 0.0;
          cmax[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtchan[j] > gmax[wt]){
                gmax[wt] = gtchan[j];
                cmax[wt] = j;
              }

          // find minimum gtvalid time
          gmin[wt] = 9999.0;
          cmin[wt] = 0;
          for (int j=0;j<32;j++)
            if ((0x1<<j) & channelMask)
              if (gtchan[j] < gmin[wt]){
                gmin[wt] = gtchan[j];
                cmin[wt] = j;
              }


          // initial printout
          if (wt == 1){
            printf("GTVALID initial results, time in ns:\n");
            printf("---------------------------------------\n");
            printf("Crate Slot Chan GTDELAY 0/1:\n");
            for (int j=0;j<32;j++){
              if ((0x1<<j) & channelMask)
                printf("%d %d %d %f %f\n",crateNum,i,j,gt_start[0][j],gt_start[1][j]);
            }
            printf("TAC 0 max at chan %d: %f\n",cmax[0],gmax[0]);
            printf("TAC 1 max at chan %d: %f\n",cmax[1],gmax[1]);
            printf("TAC 0 min at chan %d: %f\n",cmin[0],gmin[0]);
            printf("TAC 1 min at chan %d: %f\n",cmin[1],gmin[1]);
          }

          // if gt_cutoff is set, we are going to change
          // the ISETM dacs untill all the channels are
          // just below it.
          if (gtCutoff != 0){
            printf("Finding ISETM values for crate %d, slot %d TAC %d\n",
                crateNum,i,wt);
            isetm_new[0] = ISETM;
            isetm_new[1] = ISETM;
            done = 0;
            gt_temp = gmax[wt];

            while (!done){
              // load a new dac value
              error = xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm_new[wt],i);
              if (error){
                printf("Error loading Dacs. Exiting\n");
                return -1;
              }
              // get a new measure of gtvalid
              xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<cmax[wt],&result);
              error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_new[0],isetm_new[1]);
              if (error != 0){
                printf("Error getting gtdelay at slot %d, channel %d\n",i,cmax[wt]);
                return -1;
              }
              if (gt_temp <= gtCutoff){
                done = 1;
              }else{
                if (isetm_new[wt] == 255){
                  printf("warning - ISETM set to max!\n");
                  done = 1;
                }else{
                  isetm_new[wt]++;
                }
              }

            } // end while gt_temp > gt_cutoff 

            printf("\nFound ISETM value, checking new maximum\n");

            // check that we still have the max channel
            for (int j=0;j<32;j++){
              if ((0x1<<j) & channelMask){
                xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<j,&result);
                error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_new[0],isetm_new[1]);
                if (error != 0){
                  printf("Error getting gtdelay at slot %d, channel %d\n",i,j);
                  return -1;
                }
                gtchan[j] = gt_temp;
              }
            }
            // find maximum gtvalid time
            gt_max_sec = 0.0;
            chan_max_sec = 0;
            for (int j=0;j<32;j++)
              if ((0x1<<j) & channelMask)
                if (gtchan[j] > gt_max_sec){
                  gt_max_sec = gtchan[j];
                  chan_max_sec = j;
                }


            // if the maximum channel has changed
            // refind the good isetm value
            if (chan_max_sec != cmax[wt]){
              printf("Warning, second chan_max not same as first.\n");
              cmax[wt] = chan_max_sec;
              gmax[wt] = gt_max_sec;
              gt_temp = gmax[wt];
              while (gt_temp > gtCutoff){
                isetm_new[wt]++;
                error = xl3s[crateNum]->LoadsDac(d_isetm[wt],isetm_new[wt],i);
                xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,0x1<<cmax[wt],&result);
                error = GetGTDelay(crateNum,i,wt,&gt_temp,isetm_new[0],isetm_new[1]);
                if (error != 0){
                  printf("Error getting gtdelay at slot %d, channel %d\n",i,cmax[wt]);
                  return -1;
                }
              }
            } // end if channel max changed

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
                      printf("Error getting gtdelay at slot %d, channel %d\n",i,j);
                      return -1;
                    }
                    gtchan_set[wt][j] = gt_temp;
                  }
                }
                done = 1;
                // now successively turn off twiddle bits
                printf("\n");
                for (int j=0;j<32;j++){
                  if ((0x1<<j) & channelMask){
                    if ((gtchan_set[wt][j] <= gtCutoff) && (gtflag[wt][j] == 0) &&
                        (tacbits_new[wt][j] != 0x0)){
                      tacbits_new[wt][j]-=0x1; // decrement twiddle by 1
                      done = 0;
                      printf("Channel %d, %f, tacbits at %01x\n",j,gtchan_set[wt][j],tacbits_new[wt][j]);
                    }else if ((gtchan_set[wt][j] > gtCutoff) && (gtflag[wt][j] == 0)){
                      tacbits_new[wt][j] += 0x1; // go up just one
                      if (tacbits_new[wt][j] > 0x7)
                        tacbits_new[wt][j] = 0x7; //max
                      gtflag[wt][j] = 1; // this channel is as close to gt_cutoff as possible
                      printf("Channel %d ok\n",j);
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
                printf("TAC bits loaded\n");

              } // end while not done
            } // end if do_twiddle

            // we are done, save the setup
            isetm_save[wt] = isetm_new[wt];
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
                  printf("Error getting gtdelay at slot %d, channel %d\n",i,j);
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
        printf("\nCrate %d Slot %d - GTVALID FINAL results, time in ns:\n",crateNum,i);
        printf("--------------------------------------------------------\n");
        if (!twiddleOn)
          printf(" >>> ISETA0/1 = 0, no TAC twiddle bits set\n");
        printf("set up: VMAX: %hu, TACREF: %hu, ",VMAX,TACREF);
        if (twiddleOn)
          printf("ISETA: %hu\n",ISETA);
        else
          printf("ISETA: %hu\n",ISETA_NO_TWIDDLE);
        printf("Found ISETM0: %d, ISETM1: %d\n",isetm_save[0],isetm_save[1]);
        printf("Chan Tacbits GTValid 0/1:\n");
        for (int j=0;j<32;j++){
          if ((0x1<<j) & channelMask){
            printf("%d 0x%hx %f %f",
                j,tacbits_save[1][j]*16 + tacbits_save[0][j],
                gtchan_set[0][j],gtchan_set[1][j]);
            if (isetm_save[0] == ISETM || isetm_save[1] == ISETM)
              printf(">>> Warning: isetm not adjusted\n");
            else
              printf("\n");
          }
        }

        printf(">>> Maximum TAC0 GTValid - Chan %d: %f\n",
            chan_max_set[0],gt_max_set[0]);
        printf(">>> Minimum TAC0 GTValid - Chan %d: %f\n",
            chan_min_set[0],gt_min_set[0]);
        printf(">>> Maximum TAC1 GTValid - Chan %d: %f\n",
            chan_max_set[1],gt_max_set[1]);
        printf(">>> Minimum TAC1 GTValid - Chan %d: %f\n",
            chan_min_set[1],gt_min_set[1]);

        if (abs(isetm_save[1] - isetm_save[0]) > 10)
          slot_errors |= 0x1;
        for (int j=0;j<32;j++){
          if ((gtchan_set[0][j] < 0) || gtchan_set[1][j] < 0)
            chan_errors[j] = 1;
        }

        //store in DB
        if (updateDB){
          printf("updating the database\n");
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
  catch(int e){
    printf("There was a network error!\n");
  }

  printf("****************************************\n");
  return 0;
}


int GetGTDelay(int crateNum, int slotNum, int wt, float *get_gtchan, uint16_t isetm0, uint16_t isetm1)
{
  printf(".");
  fflush(stdout);

  float upper_limit, lower_limit, current_delay;
  int error, done, i;
  uint32_t result, num_read;
  XL3Packet packet;
  Command *command;
  done = 0;
  upper_limit = GTMAX;
  lower_limit = 250;
  uint32_t select_reg = FEC_SEL*slotNum;

  // set unmeasured TAC GTValid to long window time
  // that way the TAC we are looking at should fail
  // first.
  int othertac = (wt+1)%2;
  error = xl3s[crateNum]->LoadsDac(d_isetm[othertac],ISETM_LONG,slotNum);

  // find the time that the TAC stops firiing
  while (done == 0){
    // reset fifo
    xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result);
    xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);

    // binary search for GT delay
    current_delay = (upper_limit - lower_limit)/2.0 + lower_limit;
    current_delay = mtc->SetGTDelay(current_delay+GTPED_DELAY+TDELAY_EXTRA) - 
      GTPED_DELAY-TDELAY_EXTRA;

    mtc->MultiSoftGT(NGTVALID);
    usleep(500);

    xl3s[crateNum]->RW(FIFO_WRITE_PTR_R + select_reg + READ_REG,0x0,&result);
    num_read = (result & 0x000FFFFF)/3;
    if (num_read < (NGTVALID)*0.75)
      upper_limit = current_delay;
    else
      lower_limit = current_delay;

    if (upper_limit - lower_limit <= 1)
      done = 1;

  } // end while not done

  // check if we actually found a good value or
  // if we just kept going to to the upper bound
  if (upper_limit == GTMAX){
    *get_gtchan = -2;
  }else{
    // ok we know that lower limit is within the window, upper limit is outside
    // lets make sure its the right TAC failing by making the window longer and
    // seeing if the events show back up
    error = xl3s[crateNum]->LoadsDac(d_isetm[wt],ISETM_LONG,slotNum);

    // reset fifo
    xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result);
    xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);

    current_delay = mtc->SetGTDelay(upper_limit+GTPED_DELAY+TDELAY_EXTRA) - 
      GTPED_DELAY-TDELAY_EXTRA;

    mtc->MultiSoftGT(NGTVALID);
    usleep(500);

    xl3s[crateNum]->RW(FIFO_WRITE_PTR_R + select_reg + READ_REG,0x0,&result);
    num_read = (result & 0x000FFFFF)/3;
    if (num_read < (NGTVALID*0.75)){
      printf("Uh oh, still not all the events! wrong TAC failing\n");
      *get_gtchan = -1;
    }else{
      *get_gtchan = upper_limit;
    }
  }

  // set TACs back to original time
  error = xl3s[crateNum]->LoadsDac(d_isetm[0],isetm0,slotNum);
  error = xl3s[crateNum]->LoadsDac(d_isetm[1],isetm1,slotNum);

  return 0;
}
