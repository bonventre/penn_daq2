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
#include "ChinjScan.h"

int ChinjScan(int crateNum, uint32_t slotMask, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int numPedestals, float upper, float lower, int qSelect, int pedOn, int updateDB, int finalTest)
{
  lprintf("*** Starting Charge Injection Test *****\n");

  float *qhls;
  float *qhss;
  float *qlxs;
  float *tacs;
  int *scan_errors;
  int errors = 0;
  int count,crateID,ch,cell,num_events;
  uint16_t dacvalue;
  uint32_t *pmt_buffer,*pmt_iter;
  struct pedestal *ped;
  uint32_t result, select_reg;
  uint32_t default_ch_mask;
  int chinj_err[16];

  pmt_buffer = (uint32_t *) malloc( 0x20000*sizeof(uint32_t));
  ped = (struct pedestal *) malloc( 32 * sizeof(struct pedestal));
  qhls = (float *) malloc(32*16*2*26*sizeof(float));
  qhss = (float *) malloc(32*16*2*26*sizeof(float));
  qlxs = (float *) malloc(32*16*2*26*sizeof(float));
  tacs = (float *) malloc(32*16*2*26*sizeof(float));
  scan_errors = (int *) malloc(32*16*2*26*sizeof(int));
  if (pmt_buffer == NULL || ped == NULL || qhls == NULL || qhss == NULL || qlxs == NULL || tacs == NULL || scan_errors == NULL){
    lprintf("Problem mallocing! Exiting\n");
    if (pmt_buffer != NULL)
      free(pmt_buffer);
    if (ped != NULL)
      free(ped);
    if (qhls != NULL)
      free(qhls);
    if (qhss != NULL)
      free(qhss);
    if (qlxs != NULL)
      free(qlxs);
    if (tacs != NULL)
      free(tacs);
    if (scan_errors != NULL)
      free(scan_errors);
    return -1;
  }

  for (int i=0;i<16;i++){
    chinj_err[i] = 0;
    for (int j=0;j<32;j++)
      for (int k=0;k<26;k++)
        for (int l=0;l<2;l++){
          qhls[k*16*32*2+i*32*2+j*2+l] = 0;
          qhss[k*16*32*2+i*32*2+j*2+l] = 0;
          qlxs[k*16*32*2+i*32*2+j*2+l] = 0;
          tacs[k*16*32*2+i*32*2+j*2+l] = 0;
          scan_errors[k*16*32*2+i*32*2+j*2+l] = 0;
        }
  }

  try {

    for (int dac_iter=0;dac_iter<26;dac_iter++){

      dacvalue = dac_iter*10;

      for (int slot_iter = 0; slot_iter < 16; slot_iter ++){
        if ((0x1 << slot_iter) & slotMask){
          select_reg = FEC_SEL*slot_iter;
          xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
          xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result);
          xl3s[crateNum]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);
          xl3s[crateNum]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,~channelMask,&result);
        }
      }
      xl3s[crateNum]->DeselectFECs(); 

      //pt_printsend("Reset FECs\n");

      errors = 0;
      errors += xl3s[crateNum]->LoadCrateAddr(slotMask);
      if (pedOn){
        //pt_printsend("enabling pedestals.\n");
        errors += xl3s[crateNum]->SetCratePedestals(slotMask, channelMask);
      }
      xl3s[crateNum]->DeselectFECs(); 

      if (errors){
        lprintf("error setting up FEC crate for pedestals. Exiting.\n");
        free(pmt_buffer);
        free(ped);
        free(qhls);
        free(qhss);
        free(qlxs);
        free(tacs);
        free(scan_errors);
        return -1;
      }

      //setup charge injection
      xl3s[crateNum]->SetupChargeInjection(slotMask,channelMask,dacvalue);

      errors = mtc->SetupPedestals(0,pedWidth,gtDelay,DEFAULT_GT_FINE_DELAY,(0x1<<crateNum),(0x1<<crateNum));
      if (errors){
        lprintf("Error setting up MTC for pedestals. Exiting.\n");
        mtc->UnsetPedCrateMask(MASKALL);
        mtc->UnsetGTCrateMask(MASKALL);
        free(pmt_buffer);
        free(ped);
        free(qhls);
        free(qhss);
        free(qlxs);
        free(tacs);
        free(scan_errors);
        return -1;
      }

      // send the softgts
      mtc->MultiSoftGT(numPedestals*16);

      // LOOP OVER SLOTS
      for (int slot_iter = 0; slot_iter < 16; slot_iter ++){
        if ((0x1<<slot_iter) & slotMask){

          // initialize pedestal struct
          for (int i=0;i<32;i++){
            //pedestal struct
            ped[i].channelnumber = i; //channel numbers start at 0!!!
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


          /////////////////////
          // READOUT BUNDLES //
          /////////////////////

          count = xl3s[crateNum]->ReadOutBundles(slot_iter, pmt_buffer, numPedestals*32*16,1);

          //check for readout errors
          if (count <= 0){
            lprintf("there was an error in the count!\n");
            lprintf("Errors reading out MB(%2d) (errno %i)\n", slot_iter, count);
            errors+=1;
            continue;
          }else{
            //pt_printsend("MB(%2d): %5d bundles read out.\n", slot_iter, count);
          }

          if (count < numPedestals*32*16)
            errors += 1;

          //process data
          pmt_iter = pmt_buffer;

          for (int i=0;i<count;i++){
            crateID = (int) UNPK_CRATE_ID(pmt_iter);
            if (crateID != crateNum){
              lprintf("Invalid crate ID seen! (crate ID %2d, bundle %2i)\n", crateID, i);
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
                  ped[i].thiscell[j].qlxrms = sqrt(ped[i].thiscell[j].qlxrms);
                  ped[i].thiscell[j].qhsrms = sqrt(ped[i].thiscell[j].qhsrms);
                  ped[i].thiscell[j].qhlrms = sqrt(ped[i].thiscell[j].qhlrms);
                  ped[i].thiscell[j].tacrms = sqrt(ped[i].thiscell[j].tacrms);
                }else{
                  ped[i].thiscell[j].qlxrms = 0;
                  ped[i].thiscell[j].qhsrms = 0;
                  ped[i].thiscell[j].qhlrms = 0;
                  ped[i].thiscell[j].tacrms = 0;
                }
              }
            }
          }

          ///////////////////
          // PRINT RESULTS //
          ///////////////////

          lprintf("########################################################\n");
          lprintf("Slot (%2d)\n", slot_iter);
          lprintf("########################################################\n");

          for (int i = 0; i<32; i++){
            //pt_printsend("Ch Cell  #   Qhl         Qhs         Qlx         TAC\n");
            for (int j=0;j<16;j++){
              if (j == 0){
                qhls[dac_iter*16*32*2+slot_iter*32*2+i*2] = ped[i].thiscell[j].qhlbar;
                qhss[dac_iter*16*32*2+slot_iter*32*2+i*2] = ped[i].thiscell[j].qhsbar;
                qlxs[dac_iter*16*32*2+slot_iter*32*2+i*2] = ped[i].thiscell[j].qlxbar;
                tacs[dac_iter*16*32*2+slot_iter*32*2+i*2] = ped[i].thiscell[j].tacbar;
              }
              if (j == 1){
                qhls[dac_iter*16*32*2+slot_iter*32*2+i*2+1] = ped[i].thiscell[j].qhlbar;
                qhss[dac_iter*16*32*2+slot_iter*32*2+i*2+1] = ped[i].thiscell[j].qhsbar;
                qlxs[dac_iter*16*32*2+slot_iter*32*2+i*2+1] = ped[i].thiscell[j].qlxbar;
                tacs[dac_iter*16*32*2+slot_iter*32*2+i*2+1] = ped[i].thiscell[j].tacbar;
              }
              if (qSelect == 0){
                if (ped[i].thiscell[j].qhlbar < lower ||
                    ped[i].thiscell[j].qhlbar > upper) {
                  chinj_err[slot_iter]++;
                  //pt_printsend(">>>>>Qhl Extreme Value<<<<<\n");
                  if (j%2 == 0)
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2]++;
                  else
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2+1]++;
                }
              }
              else if (qSelect == 1){
                if (ped[i].thiscell[j].qhsbar < lower ||
                    ped[i].thiscell[j].qhsbar > upper) {
                  chinj_err[slot_iter]++;
                  //pt_printsend(">>>>>Qhs Extreme Value<<<<<\n");
                  if (j%2 == 0)
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2]++;
                  else
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2+1]++;
                }
              }
              else if (qSelect == 2){
                if (ped[i].thiscell[j].qlxbar < lower ||
                    ped[i].thiscell[j].qlxbar > upper) {
                  chinj_err[slot_iter]++;
                  //pt_printsend(">>>>>Qlx Extreme Value<<<<<\n");
                  if (j%2 == 0)
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2]++;
                  else
                    scan_errors[dac_iter*16*32*2+slot_iter*32*2+i*2+1]++;
                }
              }
              if (j==0){
                lprintf("%2d %3d %4d %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f %6.1f %4.1f\n",
                    dacvalue,i,ped[i].thiscell[j].per_cell,
                    ped[i].thiscell[j].qhlbar, ped[i].thiscell[j].qhlrms,
                    ped[i].thiscell[j].qhsbar, ped[i].thiscell[j].qhsrms,
                    ped[i].thiscell[j].qlxbar, ped[i].thiscell[j].qlxrms,
                    ped[i].thiscell[j].tacbar, ped[i].thiscell[j].tacrms);
              }
            }
          }

        } // end if slotmask
      } // end loop over slots


      //    if (arg.q_select == 0){
      //    pt_printsend("Qhl lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
      //    pt_printsend("Number of Qhl overflows = %d\n",chinj_err[slot_iter]);
      //    }
      //    else if (arg.q_select == 1){
      //    pt_printsend("Qhs lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
      //    pt_printsend("Number of Qhs overflows = %d\n",chinj_err[slot_iter]);
      //    }
      //    else if (arg.q_select == 2){
      //    pt_printsend("Qlx lower, Upper bounds = %f %f\n",arg.chinj_lower,arg.chinj_upper);
      //    pt_printsend("Number of Qlx overflows = %d\n",chinj_err[slot_iter]);
      //    }



      //disable trigger enables
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);

      //unset pedestalenable
      errors += xl3s[crateNum]->SetCratePedestals(slotMask, 0x0);

      xl3s[crateNum]->DeselectFECs();
    } // end loop over dacvalue

    free(pmt_buffer);
    free(ped);

    // lets update this database
    if (updateDB){
      lprintf("updating the database\n");
      for (int i=0;i<16;i++)
      {
        if ((0x1<<i) & slotMask){
          JsonNode *newdoc = json_mkobject();
          JsonNode *qhl_even = json_mkarray();
          JsonNode *qhl_odd = json_mkarray();
          JsonNode *qhs_even = json_mkarray();
          JsonNode *qhs_odd = json_mkarray();
          JsonNode *qlx_even = json_mkarray();
          JsonNode *qlx_odd = json_mkarray();
          JsonNode *tac_even = json_mkarray();
          JsonNode *tac_odd = json_mkarray();
          JsonNode *error_even = json_mkarray();
          JsonNode *error_odd = json_mkarray();
          for (int j=0;j<32;j++){
            JsonNode *qhleventemp = json_mkarray();
            JsonNode *qhloddtemp = json_mkarray();
            JsonNode *qhseventemp = json_mkarray();
            JsonNode *qhsoddtemp = json_mkarray();
            JsonNode *qlxeventemp = json_mkarray();
            JsonNode *qlxoddtemp = json_mkarray();
            JsonNode *taceventemp = json_mkarray();
            JsonNode *tacoddtemp = json_mkarray();
            JsonNode *erroreventemp = json_mkarray();
            JsonNode *erroroddtemp = json_mkarray();
            for (int k=0;k<26;k++){
              json_append_element(qhleventemp,json_mknumber(qhls[k*16*32*2+i*32*2+j*2]));	
              json_append_element(qhloddtemp,json_mknumber(qhls[k*16*32*2+i*32*2+j*2+1]));	
              json_append_element(qhseventemp,json_mknumber(qhss[k*16*32*2+i*32*2+j*2]));	
              json_append_element(qhsoddtemp,json_mknumber(qhss[k*16*32*2+i*32*2+j*2+1]));	
              json_append_element(qlxeventemp,json_mknumber(qlxs[k*16*32*2+i*32*2+j*2]));	
              json_append_element(qlxoddtemp,json_mknumber(qlxs[k*16*32*2+i*32*2+j*2+1]));	
              json_append_element(taceventemp,json_mknumber(tacs[k*16*32*2+i*32*2+j*2]));	
              json_append_element(tacoddtemp,json_mknumber(tacs[k*16*32*2+i*32*2+j*2+1]));	
              json_append_element(erroreventemp,json_mkbool(scan_errors[k*16*32*2+i*32*2+j*2]));	
              json_append_element(erroroddtemp,json_mkbool(scan_errors[k*16*32*2+i*32*2+j*2+1]));	
            }
            json_append_element(qhl_even,qhleventemp);
            json_append_element(qhl_odd,qhloddtemp);
            json_append_element(qhs_even,qhseventemp);
            json_append_element(qhs_odd,qhsoddtemp);
            json_append_element(qlx_even,qlxeventemp);
            json_append_element(qlx_odd,qlxoddtemp);
            json_append_element(tac_even,taceventemp);
            json_append_element(tac_odd,tacoddtemp);
            json_append_element(error_even,erroreventemp);
            json_append_element(error_odd,erroroddtemp);
          }
          json_append_member(newdoc,"type",json_mkstring("chinj_scan"));
          json_append_member(newdoc,"QHL_even",qhl_even);
          json_append_member(newdoc,"QHL_odd",qhl_odd);
          json_append_member(newdoc,"QHS_even",qhs_even);
          json_append_member(newdoc,"QHS_odd",qhs_odd);
          json_append_member(newdoc,"QLX_even",qlx_even);
          json_append_member(newdoc,"QLX_odd",qlx_odd);
          json_append_member(newdoc,"TAC_even",tac_even);
          json_append_member(newdoc,"TAC_odd",tac_odd);
          json_append_member(newdoc,"errors_even",error_even);
          json_append_member(newdoc,"errors_odd",error_odd);
          json_append_member(newdoc,"pass",json_mkbool(!(chinj_err[i])));
          if (finalTest){
            json_append_member(newdoc,"final_test_id",json_mkstring(finalTestIDs[crateNum][i]));	
          }
          PostDebugDoc(crateNum,i,newdoc);
        }
      }
    }

    if (errors)
      lprintf("There were %d errors\n", errors);
    else
      lprintf("No errors seen\n");

    free(qhls);
    free(qhss);
    free(qlxs);
    free(tacs);
    free(scan_errors);

  }
  catch(const char* s){
    lprintf("ChinjScan: %s\n",s);
    if (pmt_buffer != NULL)
      free(pmt_buffer);
    if (ped != NULL)
      free(ped);
    if (qhls != NULL)
      free(qhls);
    if (qhss != NULL)
      free(qhss);
    if (qlxs != NULL)
      free(qlxs);
    if (tacs != NULL)
      free(tacs);
    if (scan_errors != NULL)
      free(scan_errors);

  }

  lprintf("****************************************\n");
  return 0;
}


