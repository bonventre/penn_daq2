#include "math.h"

#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "DacNumber.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "CrateCBal.h"

int CrateCBal(int crateNum, uint32_t slotMask, uint32_t channelMask, int updateDB, int finalTest, int ecal)
{
  printf("*** Starting Crate Charge Balancing  ***\n");


  // constants
  const int max_channels = 32;
  const int num_cells = 16;
  const int max_iterations = 40;
  const float acceptable_diff = 10;
  const uint16_t low_dac_initial_setting = 50;
  const uint16_t high_dac_initial_setting = 225;
  const int vsi_test_val = 0;
  const int isetm_test_val = 85;
  const int rmp1_test_val = 100;
  const int vli_test_val = 120;
  const int rmp2_test_val = 155;
  const int vmax_test_val = 203;
  const int tacref_test_val = 72;

  uint32_t balanced_chans;
  uint32_t active_chans = channelMask;
  uint32_t orig_active_chans;

  struct pedestal x1[32],x2[32],tmp[32];
  struct pedestal x1l[32],x2l[32],tmpl[32];

  int x1_bal[32],x2_bal[32],tmp_bal[32],bestguess_bal[32]; // dac settings
  float f1[32],f2[32]; // the charge that we are balancing

  struct channel_params chan_param[32];

  int iterations,return_value = 0;

  float fmean1,fmean2;
  int vbal_temp[2][32*16];
  uint32_t result;
  uint32_t *pmt_buf;
  int num_dacs;
  uint32_t dac_nums[50];
  uint32_t dac_values[50];
  uint32_t slot_nums[50];

  int error_flags[32];
  for (int ef=0;ef<32;ef++)
    error_flags[ef] = 0;

  // malloc
  printf("about to malloc\n");
  pmt_buf = (uint32_t *) malloc(0x100000*sizeof(uint32_t));
  printf("malloced\n");
  if (pmt_buf == NULL){
    printf("Problem mallocing space for pedestals. Exiting\n");
    return -1;
  }

  try{

    // set up pulser for soft GT mode
    if (mtc->SetupPedestals(0,50,125,0,(0x1<<crateNum)+MSK_TUB,(0x1<<crateNum)+MSK_TUB)){
      printf("problem setting up pedestals on mtc. Exiting\n");
      free(pmt_buf);
      return -1;
    }

    // loop over slots
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        uint32_t select_reg = FEC_SEL*i;
        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0xF,&result);

        printf("--------------------------------\n");
        printf("Balancing Crate %d, Slot %d\n",crateNum,i);

        // initialize variables
        int skip_slot = 0;
        iterations = 0;
        num_dacs = 0;
        balanced_chans = 0x0;
        for (int j=0;j<32;j++){
          chan_param[j].test_status = 0x0; // zero means test not passed
          chan_param[j].hi_balanced = 0; // high not balanced
          chan_param[j].low_balanced = 0; // low not balanced
          chan_param[j].high_gain_balance = 0;
          chan_param[j].low_gain_balance = 0;
        }
        orig_active_chans = active_chans;

        // set up timing
        // we will set VSI and VLI to a long time for test
        for (int j=0;j<8;j++){
          dac_nums[num_dacs] = d_rmp[j];
          dac_values[num_dacs] = rmp2_test_val;
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_rmpup[j];
          dac_values[num_dacs] = rmp1_test_val;
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vsi[j];
          dac_values[num_dacs] = vsi_test_val;
          slot_nums[num_dacs] = i;
          num_dacs++;
          dac_nums[num_dacs] = d_vli[j];
          dac_values[num_dacs] = vli_test_val;
          slot_nums[num_dacs] = i;
          num_dacs++;
        }
        // now CMOS timing for GTValid
        for (int j=0;j<2;j++){
          dac_nums[num_dacs] = d_isetm[j];
          dac_values[num_dacs] = isetm_test_val;
          slot_nums[num_dacs] = i;
          num_dacs++;
        }
        dac_nums[num_dacs] = d_tacref;
        dac_values[num_dacs] = tacref_test_val;
        slot_nums[num_dacs] = i;
        num_dacs++;
        dac_nums[num_dacs] = d_vmax;
        dac_values[num_dacs] = vmax_test_val;
        slot_nums[num_dacs] = i;
        num_dacs++;
        // now lets load these dacs
        if (xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums) != 0){
          printf("Error loading dacs. Skipping slot\n");
          for (int j=0;j<32;j++)
            error_flags[j] = 5;
          skip_slot = 1;
        }
        num_dacs = 0;

        // loop over high and low gain
        // first loop balanced qhs with qhl
        // second loop balances qlx (normal) with qlx (LGI set) 
        if (skip_slot == 0){
          for (int wg=0;wg<2;wg++){
            // initialize
            active_chans = orig_active_chans;
            // rezero
            for (int j=0;j<32;j++){
              f1[j] = 0;
              f2[j] = 0;
              x1_bal[j] = low_dac_initial_setting;
              x2_bal[j] = high_dac_initial_setting;
            }


            // calculate min balance
            // set dacs to minimum
            for (int j=0;j<32;j++){
              if ((0x1<<j) & active_chans){
                if (wg == 0)
                  dac_nums[num_dacs] = d_vbal_hgain[j];
                else
                  dac_nums[num_dacs] = d_vbal_lgain[j];
                dac_values[num_dacs] = x1_bal[j];
                slot_nums[num_dacs] = i;
                num_dacs++;
              }
            }
            if (xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums)){
              printf("Error loading dacs. Skipping slot\n");
              for (int j=0;j<32;j++)
                error_flags[j] = 5;
              skip_slot = 1;
              break;
            }
            num_dacs = 0;
            // get pedestal data
            if (GetPedestal(crateNum,i,channelMask,x1,chan_param,pmt_buf)){
              printf("Error during pedestal running or reading. Skipping slot\n");
              for (int j=0;j<32;j++)
                error_flags[j] = 5;
              skip_slot = 1;
              break;
            }
            // if low gain do again with LGI bit set
            if (wg == 1){
              xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result); 
              if (GetPedestal(crateNum,i,channelMask,x1l,chan_param,pmt_buf)){
                printf("Error during pedestal running or reading. Skipping slot\n");
                for (int j=0;j<32;j++)
                  error_flags[j] = 5;
                skip_slot = 1;
                break;
              }
              xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result); 
            }

            // calculate max balance
            // set dacs to maximum
            for (int j=0;j<32;j++){
              if ((0x1<<j) & active_chans){
                if (wg == 0)
                  dac_nums[num_dacs] = d_vbal_hgain[j];
                else
                  dac_nums[num_dacs] = d_vbal_lgain[j];
                dac_values[num_dacs] = x2_bal[j];
                slot_nums[num_dacs] = i;
                num_dacs++;
              }
            }
            if (xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums)){
              printf("Error loading dacs. Skipping slot\n");
              for (int j=0;j<32;j++)
                error_flags[j] = 5;
              skip_slot = 1;
              break;
            }
            num_dacs = 0;
            // get pedestal data
            if (GetPedestal(crateNum,i,channelMask,x2,chan_param,pmt_buf)){
              printf("Error during pedestal running or reading. Skipping slot\n");
              for (int j=0;j<32;j++)
                error_flags[j] = 5;
              skip_slot = 1;
              break;
            }
            // if low gain do again with LGI bit set
            if (wg == 1){
              xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result); 
              if (GetPedestal(crateNum,i,channelMask,x2l,chan_param,pmt_buf)){
                printf("Error during pedestal running or reading. Skipping slot\n");
                for (int j=0;j<32;j++)
                  error_flags[j] = 5;
                skip_slot = 1;
                break;
              }
              xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result); 
            }

            // end setting initial high and low value

            iterations = 0;
            balanced_chans = 0x0;

            // we will now loop until we converge and are balanced
            do{
              // make sure we arent stuck forever
              if (iterations++ > max_iterations){
                printf("Too many interations, exiting with some channels unbalanced.\n");
                //printf("Making best guess for unbalanced channels\n");
                for (int j=0;j<32;j++)
                  if (wg == 0){
                    if (chan_param[j].hi_balanced == 0)
                      chan_param[j].high_gain_balance == bestguess_bal[j];
                  }else              
                    if (chan_param[j].low_balanced == 0)
                      chan_param[j].low_gain_balance == bestguess_bal[j];
                break;
              }

              // loop over channels
              for (int j=0;j<32;j++){
                // if this channel is active and is not yet balanced
                int is_balanced = wg == 0 ? chan_param[j].hi_balanced : chan_param[j].low_balanced;
                if (((0x1<<j) & active_chans) && (is_balanced == 0)){
                  fmean1 = 0;
                  fmean2 = 0;
                  if (wg == 0){
                    // find the average difference between qhl and qhs
                    for (int k=0;k<16;k++){
                      fmean1 += x1[j].thiscell[k].qhlbar-x1[j].thiscell[k].qhsbar;
                      fmean2 += x2[j].thiscell[k].qhlbar-x2[j].thiscell[k].qhsbar;
                    }
                  }else{
                    // find the average difference between qhl and qhs
                    for (int k=0;k<16;k++){
                      fmean1 += x1[j].thiscell[k].qlxbar-x1l[j].thiscell[k].qlxbar;
                      fmean2 += x2[j].thiscell[k].qlxbar-x2l[j].thiscell[k].qlxbar;
                    }
                  }
                  f1[j] = fmean1/16;
                  f2[j] = fmean2/16;
                  // make sure we straddle best fit point
                  // i.e. the both have the sign on first run
                  if (((f1[j]*f2[j]) > 0.0) && (iterations == 1)){
                    printf("Error: channel %d does not appear balanceable. (%f, %f)\n",
                        j,f1[j],f2[j]);
                    // turn this channel off and go on
                    if (fabs(f1[j]) < fabs(f2[j])){
                      if (wg == 0)
                        chan_param[j].high_gain_balance = x1_bal[j];
                      else
                        chan_param[j].low_gain_balance = x1_bal[j];
                    }else{
                      if (wg == 0)
                        chan_param[j].high_gain_balance = x2_bal[j];
                      else
                        chan_param[j].low_gain_balance = x2_bal[j];
                    }
                    active_chans &= ~(0x1<<j);
                    return_value += 100;
                    break;
                  }
                  // check if either high or low was balanced
                  if (fabs(f2[j]) < acceptable_diff){
                    balanced_chans |= 0x1<<j;
                    if (wg == 0){
                      chan_param[j].hi_balanced = 1;
                      chan_param[j].high_gain_balance = x2_bal[j];
                    }else{
                      chan_param[j].low_balanced = 1;
                      chan_param[j].low_gain_balance = x2_bal[j];
                    }
                    active_chans &= ~(0x1<<j);
                  }else if (fabs(f1[j]) < acceptable_diff){
                    balanced_chans |= 0x1<<j;
                    if (wg == 0){
                      chan_param[j].hi_balanced = 1;
                      chan_param[j].high_gain_balance = x1_bal[j];
                    }else{
                      chan_param[j].low_balanced = 1;
                      chan_param[j].low_gain_balance = x1_bal[j];
                    }
                    active_chans &= ~(0x1<<j);
                  }else{
                    // still not balanced
                    // pick new points to test
                    tmp_bal[j] = x1_bal[j] + (x2_bal[j]-x1_bal[j])*(f1[j]/(f1[j]-f2[j]));

                    // keep track of best guess
                    if (fabs(f1[j] < fabs(f2[j])))
                      bestguess_bal[j] = x1_bal[j];
                    else
                      bestguess_bal[j] = x2_bal[j];

                    // make sure we arent stuck
                    if (tmp_bal[j] == x2_bal[j]){
                      printf("channel %d in local trap. Nudging\n",j);
                      int kick = (int) (rand()%35) + 150;
                      tmp_bal[j] = (tmp_bal[j] >= 45) ? (tmp_bal[j]-kick) : (tmp_bal[j] + kick);
                    }

                    // make sure we stay withing bounds
                    if (tmp_bal[j] > 255)
                      tmp_bal[j] = 255;
                    else if (tmp_bal[j] < 0)
                      tmp_bal[j] = 0;

                    if (wg == 0)
                      dac_nums[num_dacs] = d_vbal_hgain[j];
                    else
                      dac_nums[num_dacs] = d_vbal_lgain[j];
                    dac_values[num_dacs] = tmp_bal[j];
                    slot_nums[num_dacs] = i;
                    num_dacs++;

                    // now we loop through the rest of the channels
                    // and build up all the dac values we are going
                    // to set before running pedestals again

                  } // end if balanced or not
                } // end if active and not balanced
              } // end loop over channels

              // we have new pedestal values for each channel
              // lets load them up
              if (xl3s[crateNum]->MultiLoadsDac(num_dacs,dac_nums,dac_values,slot_nums)){
                printf("Error loading dacs. Skipping slot\n");
                for (int j=0;j<32;j++)
                  error_flags[j] = 5;
                skip_slot = 1;
                break;
              }
              num_dacs = 0;

              // lets do a ped run with the new balance
              // if there are still channels left to go
              if (active_chans != 0x0){
                if (GetPedestal(crateNum,i,channelMask,tmp,chan_param,pmt_buf)){
                  printf("Error during pedestal running or reading. Skipping slot\n");
                  for (int j=0;j<32;j++)
                    error_flags[j] = 5;
                  skip_slot = 1;
                  break;
                }

                if (wg == 1){
                  xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x1,&result); 
                  if (GetPedestal(crateNum,i,channelMask,tmpl,chan_param,pmt_buf)){
                    printf("Error during pedestal running or reading. Skipping slot\n");
                    for (int j=0;j<32;j++)
                      error_flags[j] = 5;
                    skip_slot = 1;
                    break;
                  }
                  xl3s[crateNum]->RW(CMOS_LGISEL_R + select_reg + WRITE_REG,0x0,&result); 
                }

                // now update the two points
                for (int j=0;j<32;j++){
                  if ((0x1<<j) & active_chans){
                    if (wg == 0){
                      x1[j] = x2[j];
                      x1_bal[j] = x2_bal[j];
                      x2[j] = tmp[j];
                      x2_bal[j] = tmp_bal[j];
                    }else{
                      x1[j] = x2[j];
                      x1l[j] = x2l[j];
                      x1_bal[j] = x2_bal[j];
                      x2[j] = tmp[j];
                      x2l[j] = tmpl[j];
                      x2_bal[j] = tmp_bal[j];
                    }
                  }
                }
              } // end if active_chans != 0x0

            } while (balanced_chans != orig_active_chans); // loop until all balanced

            if (skip_slot)
              break;

          } // end loop over gains
        } // end if skip_slot == 0

        active_chans = orig_active_chans;
        printf("\nFinal VBAL table:\n");

        // print out results
        for (int j=0;j<32;j++){
          if ((0x1<<j) & active_chans){
            // if fully balanced
            if ((chan_param[j].hi_balanced == 1) && (chan_param[j].low_balanced == 1)){
              printf("Ch %2i High: %3i. low; %3i. -> Balanced.\n",
                  j,chan_param[j].high_gain_balance,chan_param[j].low_gain_balance);
              // check for extreme values
              if ((chan_param[j].high_gain_balance == 255) ||
                  (chan_param[j].high_gain_balance == 0)){
                chan_param[j].high_gain_balance = 150;
                printf(">>>High gain balance extreme, setting to 150.\n");
                error_flags[j] = 1;
              }
              if ((chan_param[j].low_gain_balance == 255) ||
                  (chan_param[j].low_gain_balance == 0)){
                chan_param[j].low_gain_balance = 150;
                printf(">>>High gain balance extreme, setting to 150.\n");
                error_flags[j] = 1;
              }
              if ((chan_param[j].high_gain_balance > 225) ||
                  (chan_param[j].high_gain_balance < 50) ||
                  (chan_param[j].low_gain_balance > 255) ||
                  (chan_param[j].low_gain_balance < 50)){
                printf(">>> Warning: extreme balance value.\n");
                error_flags[j] = 2;
              }
            }
            // if partially balanced
            else if ((chan_param[j].hi_balanced == 1) || (chan_param[j].low_balanced == 1)){
              error_flags[j] = 3;
              printf("Ch %2i High: %3i. Low: %3i -> Partially balanced, "
                  "will set extreme to 150\n",
                  j,chan_param[j].high_gain_balance,chan_param[j].low_gain_balance);
              if ((chan_param[j].high_gain_balance == 255) ||
                  (chan_param[j].high_gain_balance == 0))
                chan_param[j].high_gain_balance = 150;
              if ((chan_param[j].low_gain_balance == 255) ||
                  (chan_param[j].low_gain_balance == 0))
                chan_param[j].low_gain_balance = 150;
            }
            // if unbalanced
            else{
              error_flags[j] = 4;
              printf("Ch %2i                       -> Unbalanced, set to 150\n",j);
              chan_param[j].high_gain_balance = 150;
              chan_param[j].low_gain_balance = 150;
              return_value++;
            } // end switch over balanced state
          } // end if active chan
        } // end loop over chans

        xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0xF,&result);
        xl3s[crateNum]->DeselectFECs();

        // now update database
        if (updateDB){
          printf("updating the database\n");
          JsonNode *newdoc = json_mkobject();
          json_append_member(newdoc,"type",json_mkstring("crate_cbal"));

          JsonNode *channels = json_mkarray();
          int pass_flag = 1;

          for (int j=0;j<32;j++){
            JsonNode *one_chan = json_mkobject();
            json_append_member(one_chan,"id",json_mknumber((double) j));
            json_append_member(one_chan,"vbal_high",
                json_mknumber((double)chan_param[j].high_gain_balance));
            json_append_member(one_chan,"vbal_low",
                json_mknumber((double)chan_param[j].low_gain_balance));
            json_append_member(one_chan,"errors",json_mkbool(error_flags[j]));
            if (error_flags[j] == 0)
              json_append_member(one_chan,"error_flags",json_mkstring("none"));
            else if (error_flags[j] == 1)
              json_append_member(one_chan,"error_flags",json_mkstring("Extreme balance set to 150"));
            else if (error_flags[j] == 2)
              json_append_member(one_chan,"error_flags",json_mkstring("Extreme balance values"));
            else if (error_flags[j] == 3)
              json_append_member(one_chan,"error_flags",json_mkstring("Partially balanced"));
            else if (error_flags[j] == 4)
              json_append_member(one_chan,"error_flags",json_mkstring("Unbalanced, set to 150"));
            if (error_flags[j] != 0)
              pass_flag = 0;
            json_append_element(channels,one_chan);
          }
          json_append_member(newdoc,"channels",channels);

          if (return_value != 0)
            pass_flag = 0;
          json_append_member(newdoc,"pass",json_mkbool(pass_flag));

          if (finalTest)
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          if (ecal)
            json_append_member(newdoc,"ecal_id",json_mkstring(ecalID));	
          PostDebugDoc(crateNum,i,newdoc);
          json_delete(newdoc); // delete the head
        }


      } // end if slot mask
    } // end loop over slots

  }
  catch(const char* s){
    printf("CrateCBal: %s\n",s);
  }

  printf("End of crate_cbal\n");
  printf("****************************************\n");

  free(pmt_buf);
  return 0;
}

int GetPedestal(int crateNum, int slotNum, uint32_t channelMask, struct pedestal *pedestals, struct channel_params *chan_params, uint32_t *pmt_buf)
{
  printf(".");
  fflush(stdout);

  uint32_t result;
  uint32_t select_reg = slotNum*FEC_SEL;

  float max_rms_qlx = 2.0;
  float max_rms_qhl = 2.0;
  float max_rms_qhs = 10.0;
  float max_rms_tac = 3.0;
  int num_pulses = 25*16;
  int min_num_words = 0;
  for (int i=0;i<32;i++)
    if ((0x1<<i) & channelMask)
      min_num_words += (num_pulses-25)*3; //??
  int max_errors = 250;

  int words_in_mem;
  ParsedBundle pmt_data;

  if (pedestals == 0){
    printf("Error: null pointer passed to GetPedestal! Exiting\n");
    return 666;
  }
  for (int i=0;i<32;i++){
    pedestals[i].channelnumber = i;
    pedestals[i].per_channel = 0;
    for (int j=0;j<16;j++){
      pedestals[i].thiscell[j].cellno = j;
      pedestals[i].thiscell[j].per_cell = 0;
      pedestals[i].thiscell[j].qlxbar = 0;
      pedestals[i].thiscell[j].qlxrms = 0;
      pedestals[i].thiscell[j].qhlbar = 0;
      pedestals[i].thiscell[j].qhlbar = 0;
      pedestals[i].thiscell[j].qhsrms = 0;
      pedestals[i].thiscell[j].qhsrms = 0;
      pedestals[i].thiscell[j].tacbar = 0;
      pedestals[i].thiscell[j].tacrms = 0;
    }
  }

  // end initialization

  // reset board - not full reset, just CMOS and fifo
  xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0xE,&result);
  xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);
  // enable the appropriate pedestals
  xl3s[crateNum]->RW(PED_ENABLE_R + select_reg + WRITE_REG,channelMask,&result);

  // now pulse the pulser num_pulses times
  mtc->MultiSoftGT(num_pulses);

  // sleep so that sequencer has time to do its shit
  usleep(5000);

  // check that we have enough data in memory
  xl3s[crateNum]->RW(FIFO_DIFF_PTR_R + select_reg + READ_REG,0x0,&result);
  result &= 0x000FFFFF;

  // if there are less bundles than there are supposed to be
  // then read out whats there minus a fudge factor ??
  if (result <= 32*3*num_pulses)
    words_in_mem = result > 100 ? result-100 : result;
  else
    words_in_mem = 32*3*num_pulses;

  // if it is too low, abort
  if (words_in_mem < min_num_words){
    printf("Less than %d bundles in memory (there are %d). Exiting\n",
        min_num_words,words_in_mem);
    return 10;
  }

  // now read out memory
  XL3Packet packet;
  ReadPedestalsArgs *args = (ReadPedestalsArgs *) packet.payload;
  ReadPedestalsResults *results = (ReadPedestalsResults *) packet.payload;
  int reads_left = words_in_mem;
  int this_read;
  while (reads_left != 0){
    if (reads_left > MAX_FEC_COMMANDS-1000)
      this_read = MAX_FEC_COMMANDS-1000;
    else
      this_read = reads_left;
    packet.header.packetType = READ_PEDESTALS_ID;
    args->slot = slotNum;
    args->reads = this_read;
    SwapLongBlock(args,sizeof(ReadPedestalsArgs)/sizeof(uint32_t));
    xl3s[crateNum]->SendCommand(&packet);
    SwapLongBlock(results,sizeof(ReadPedestalsResults)/sizeof(uint32_t));
    this_read = results->readsQueued;

    if (this_read > 0){
      // now wait for the data to come
      xl3s[crateNum]->GetMultiFCResults(this_read, packet.header.packetNum, pmt_buf+(words_in_mem-reads_left));
      reads_left -= this_read;
    }
  }

  // parse charge information
  for (int i=0;i<words_in_mem;i+=3){
    pmt_data = ParseBundle(pmt_buf+i);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qlxbar += pmt_data.ADC_Qlx;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhlbar += pmt_data.ADC_Qhl;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhsbar += pmt_data.ADC_Qhs;
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].tacbar += pmt_data.ADC_TAC;

    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qlxrms +=
      pow(pmt_data.ADC_Qlx,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhlrms +=
      pow(pmt_data.ADC_Qhl,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].qhsrms +=
      pow(pmt_data.ADC_Qhs,2);
    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].tacrms +=
      pow(pmt_data.ADC_TAC,2);

    pedestals[pmt_data.ChannelID].thiscell[pmt_data.CMOSCellID].per_cell++;
    pedestals[pmt_data.ChannelID].per_channel++;
  }

  // final step of calculation
  for (int i=0;i<32;i++){
    if (pedestals[i].per_channel > 0){
      chan_params[i].test_status |= PED_TEST_TAKEN | PED_CH_HAS_PEDESTALS |
        PED_RMS_TEST_PASSED | PED_PED_WITHIN_RANGE;
      chan_params[i].test_status &= ~(PED_DATA_NO_ENABLE | PED_TOO_FEW_PER_CELL);

      for (int j=0;j<16;j++){
        int num_events = pedestals[i].thiscell[j].per_cell;
        if (((num_events*16.0/num_pulses) > 1.1) || (num_events*16.0/num_pulses < 0.9)){
          chan_params[i].test_status |= PED_TOO_FEW_PER_CELL;
          continue;
        }
        if (num_events > 1){
          pedestals[i].thiscell[j].qlxbar = pedestals[i].thiscell[j].qlxbar / num_events;
          pedestals[i].thiscell[j].qhlbar = pedestals[i].thiscell[j].qhlbar / num_events;
          pedestals[i].thiscell[j].qhsbar = pedestals[i].thiscell[j].qhsbar / num_events;
          pedestals[i].thiscell[j].tacbar = pedestals[i].thiscell[j].tacbar / num_events;

          pedestals[i].thiscell[j].qlxrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qlxrms / num_events -
             pow(pedestals[i].thiscell[j].qlxbar,2));
          pedestals[i].thiscell[j].qhlrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qhlrms / num_events -
             pow(pedestals[i].thiscell[j].qhlbar,2));
          pedestals[i].thiscell[j].qhsrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].qhsrms / num_events -
             pow(pedestals[i].thiscell[j].qhsbar,2));
          pedestals[i].thiscell[j].tacrms = num_events / (num_events-1)*
            (pedestals[i].thiscell[j].tacrms / num_events -
             pow(pedestals[i].thiscell[j].tacbar,2));

          pedestals[i].thiscell[j].qlxrms = sqrt(pedestals[i].thiscell[j].qlxrms);
          pedestals[i].thiscell[j].qhlrms = sqrt(pedestals[i].thiscell[j].qhlrms);
          pedestals[i].thiscell[j].qhsrms = sqrt(pedestals[i].thiscell[j].qhsrms);
          pedestals[i].thiscell[j].tacrms = sqrt(pedestals[i].thiscell[j].tacrms);

          if ((pedestals[i].thiscell[j].qlxrms > max_rms_qlx) ||
              (pedestals[i].thiscell[j].qhlrms > max_rms_qhl) ||
              (pedestals[i].thiscell[j].qhsrms > max_rms_qhs) ||
              (pedestals[i].thiscell[j].tacrms > max_rms_tac))
            chan_params[i].test_status &= ~PED_RMS_TEST_PASSED;
        }else{
          chan_params[i].test_status &= ~PED_CH_HAS_PEDESTALS;
        }
      } // end loop over cells
    } // end if channel has pedestals
  } // end loop over channels
  return 0;
}

ParsedBundle ParseBundle(uint32_t *buffer)
{
  short i;
  unsigned short triggerWord2;

  unsigned short ValADC0, ValADC1, ValADC2, ValADC3;
  unsigned short signbit0, signbit1, signbit2, signbit3;
  ParsedBundle GetBundle;

  /*initialize PMTBundle to all zeros */
  // display the lower and the upper bits separately
  GetBundle.GlobalTriggerID = 0;
  GetBundle.GlobalTriggerID2 = 0;
  GetBundle.ChannelID = 0;
  GetBundle.CrateID = 0;
  GetBundle.BoardID = 0;
  GetBundle.CMOSCellID = 0;
  GetBundle.ADC_Qlx = 0;
  GetBundle.ADC_Qhs = 0;
  GetBundle.ADC_Qhl = 0;
  GetBundle.ADC_TAC = 0;
  GetBundle.CGT_ES16 = 0;
  GetBundle.CGT_ES24 = 0;
  GetBundle.Missed_Count = 0;
  GetBundle.NC_CC = 0;
  GetBundle.LGI_Select = 0;
  GetBundle.CMOS_ES16 = 0;

  GetBundle.BoardID = GetBits(*buffer, 29, 4);		// FEC32 card ID

  GetBundle.CrateID = GetBits(*buffer, 25, 5);		// Crate ID

  GetBundle.ChannelID = GetBits(*buffer, 20, 5);	// CMOS Channel ID


  // lower 16 bits of global trigger ID
  //triggerWord = GetBits(TempVal,15,16);  

  // lower 16 bits of the                       
  GetBundle.GlobalTriggerID = GetBits(*buffer, 15, 16);
  // global trigger ID
  GetBundle.CGT_ES16 = GetBits(*buffer, 30, 1);
  GetBundle.CGT_ES24 = GetBits(*buffer, 31, 1);

  // now get ADC output and peel off the corresponding values and
  // convert to decimal
  // ADC0= Q_low gain,  long integrate (Qlx)
  // ADC1= Q_high gain, short integrate time (Qhs)
  // ADC2= Q_high gain, long integrate time  (Qhl)
  // ADC3= TAC

  GetBundle.CMOSCellID = GetBits(*(buffer+1), 15, 4);	// CMOS Cell number

  signbit0 = GetBits(*(buffer+1), 11, 1);
  signbit1 = GetBits(*(buffer+1), 27, 1);
  ValADC0 = GetBits(*(buffer+1), 10, 11);
  ValADC1 = GetBits(*(buffer+1), 26, 11);

  // ADC values are in 2s complement code 
  if (signbit0 == 1)
    ValADC0 = ValADC0 - 2048;
  if (signbit1 == 1)
    ValADC1 = ValADC1 - 2048;

  //Add 2048 offset to ADC0-1 so range is from 0 to 4095 and unsigned
  GetBundle.ADC_Qlx = (unsigned short) (ValADC0 + 2048);
  GetBundle.ADC_Qhs = (unsigned short) (ValADC1 + 2048);

  GetBundle.Missed_Count = GetBits(*(buffer+1), 28, 1);;
  GetBundle.NC_CC = GetBits(*(buffer+1), 29, 1);;
  GetBundle.LGI_Select = GetBits(*(buffer+1), 30, 1);;
  GetBundle.CMOS_ES16 = GetBits(*(buffer+1), 31, 1);;

  signbit2 = GetBits(*(buffer+2), 11, 1);
  signbit3 = GetBits(*(buffer+2), 27, 1);
  ValADC2 = GetBits(*(buffer+2), 10, 11);
  ValADC3 = GetBits(*(buffer+2), 26, 11);

  // --------------- the full concatanated global trigger ID --------------
  //            for (i = 4; i >= 1 ; i--){
  //                    if ( GetBits(TempVal,(15 - i + 1),1) )
  //                            triggerWord |= ( 1UL << (19 - i + 1) );
  //            }
  //            for (i = 4; i >= 1 ; i--){
  //                    if ( GetBits(TempVal,(31 - i + 1),1) )
  //                            triggerWord |= ( 1UL << (23 - i + 1) );
  //            }
  // --------------- the full concatanated global trigger ID --------------

  triggerWord2 = GetBits(*(buffer+2), 15, 4);	// Global Trigger bits 19-16

  for (int i = 4; i >= 1; i--) {
    if (GetBits(*(buffer+2), (31 - i + 1), 1))
      triggerWord2 |= (1UL << (7 - i + 1));
  }

  // upper 8 bits of Global Trigger ID 5/27/96
  GetBundle.GlobalTriggerID2 = triggerWord2;

  // ADC values are in 2s complement code 
  if (signbit2 == 1)
    ValADC2 = ValADC2 - 2048;
  if (signbit3 == 1)
    ValADC3 = ValADC3 - 2048;

  // Add 2048 offset to ADC2-3 so range is from 0 to 4095 and unsigned
  GetBundle.ADC_Qhl = (unsigned short) (ValADC2 + 2048);
  GetBundle.ADC_TAC = (unsigned short) (ValADC3 + 2048);

  return GetBundle;
}

