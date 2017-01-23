#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "TriggerScan.h"

int TriggerScan(uint32_t crateMask, uint32_t *slotMasks, int triggerSelect, int dacSelect, int maxNhit, int minThresh, const char* fileName, int quickMode)
{
  lprintf("*** Starting Trigger Scan  *************\n");

  uint32_t select_reg,result,beforegt,aftergt;
  int num_fecs = 0;
  int min_nhit = 0;
  int last_zero, one_count,noise_count;
  float values[10000];
  uint32_t pedestals[MAX_XL3_CON][16];
  uint16_t counts[14];
  for (int i=0;i<14;i++)
    counts[i] = 10;

  FILE *file = fopen(fileName,"w");
  if (file == (FILE *) NULL){
    lprintf("Failed to open file!\n");
    return -1;
  }

  try {

    lprintf("Starting a trigger scan.\n");
    int errors = mtc->SetupPedestals(0, DEFAULT_PED_WIDTH, DEFAULT_GT_DELAY,0,
        crateMask,crateMask);
    if (errors){
      lprintf("Error setting up MTC for pedestals. Exiting\n");
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);
      fclose(file);
      return -1;
    }

    for (int i=0;i<MAX_XL3_CON;i++)
      if ((0x1<<i) & crateMask)
        for (int j=0;j<16;j++)
          if ((0x1<<j) & slotMasks[i]){
            xl3s[i]->RW(PED_ENABLE_R + FEC_SEL*j + WRITE_REG,0x0,&result);
            num_fecs++;
            pedestals[i][j] = 0x0;
          }

    // now we see our max number of nhit
    if (maxNhit == 0){
      maxNhit = num_fecs*32;
    }else if (maxNhit > num_fecs*32){
      lprintf("You dont have enough fecs to test nhit that high!\n");
      maxNhit = num_fecs*32;
      lprintf("Testing nhit up to %d\n",maxNhit);
    }

    // we loop over threshold, coming down from max
    for (int ithresh=0;ithresh<4095-minThresh;ithresh++){
      // if quick mode, do the first 50 then only every 10
      if (ithresh > 50 && quickMode == 1){ithresh += 9;}
      if (dacSelect >= 0)
        counts[dacSelect] = 4095-ithresh;
      else
        counts[triggerSelect-1] = 4095-ithresh;

      // now disable triggers while programming dacs
      // so we dont trigger off noise
      mtc->UnsetGTMask(0xFFFFFFFF);
      mtc->LoadMTCADacsByCounts(counts);
      usleep(500);
      mtc->SetGTMask(0x1<<(triggerSelect-1));

      for (int i=0;i<10000;i++)
        values[i] = -1;
      last_zero = 0;
      noise_count = 0;
      one_count = 0;

      // now we loop over nhit
      // we loop over the small subset that interests us
      for (int inhit=min_nhit;inhit<maxNhit;inhit++){
        // we need to get our pedestals set up right
        // first we set up all the fully on fecs
        int full_fecs = inhit/32;
        int unfull_fec = inhit%32;
        uint32_t unfull_pedestal = 0x0;
        for (int i=0;i<unfull_fec;i++)
          unfull_pedestal |= 0x1<<i;

        for (int icrate=0;icrate<MAX_XL3_CON;icrate++){
          if ((0x1<<icrate) & crateMask){
            for (int ifec=0;ifec<16;ifec++){
              if ((0x1<<ifec) & slotMasks[icrate]){
                if (pedestals[icrate][ifec] == 0xFFFFFFFF){
                  if (full_fecs > 0){
                    // we should keep this one fully on
                    full_fecs--;
                  }else if (full_fecs == 0){
                    // this one gets the leftovers
                    full_fecs--;
                    pedestals[icrate][ifec] = unfull_pedestal;
                    xl3s[icrate]->RW(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],&result);
                  }else{
                    // turn this one back off
                    pedestals[icrate][ifec] = 0x0;
                    xl3s[icrate]->RW(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],&result);
                  }
                }else{
                  if (full_fecs > 0){
                    // turn this one back fully on
                    full_fecs--;
                    pedestals[icrate][ifec] = 0xFFFFFFFF;
                    xl3s[icrate]->RW(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],&result);
                  }else if (full_fecs == 0){
                    // this one should get the leftovers
                    full_fecs--;
                    pedestals[icrate][ifec] = unfull_pedestal;
                    xl3s[icrate]->RW(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],&result);
                  }else if (pedestals[icrate][ifec] == 0x0){
                    // this one can stay off
                  }else{
                    // turn this one fully off
                    pedestals[icrate][ifec] = 0x0;
                    xl3s[icrate]->RW(PED_ENABLE_R + ifec*FEC_SEL + WRITE_REG,pedestals[icrate][ifec],&result);
                  }
                }
              } // end if in slot mask
            } // end loop over fecs
          } // end if in crate mask
        } // end loop over crates

        // we are now sitting at the correct nhit
        // and we have the right threshold set
        // lets do this trigger check thinger

        // get initial gt count
        mtc->RegRead(MTCOcGtReg,&beforegt);

        // send 500 pulses
        mtc->MultiSoftGT(500);

        // now get final gt count
        mtc->RegRead(MTCOcGtReg,&aftergt);

        // top bits are junk
        uint32_t diff = (aftergt & 0x00FFFFFF) - (beforegt & 0x00FFFFFF);
        values[inhit] = (float) diff / 500.0;

        // we will start at an nhit based on where
        // we start seeing triggers
        if (values[inhit] == 0)
          last_zero = inhit;

        // we will stop at an nhit based on where
        // we hit the triangle of bliss
        if (values[inhit] > 0.9 && values[inhit] < 1.1)
          one_count++;

        // we will also stop if we are stuck in the
        // noise for too long meaning we arent at a
        // high enough threshold to see the triangle
        // of bliss
        if (values[inhit] > 1.2)
          noise_count++;

        if (one_count > 5 || noise_count > 25){
          // we are done with this threshold
          min_nhit = last_zero < 5 ? 0 : last_zero-5;
          break;
        }

      } // end loop over nhit

      // now write out this thresholds worth of results
      // to file
      for (int i=0;i<10000;i++){
        // only print out nhit we tested
        if (values[i] >= 0){
          fprintf(file,"%d %d %f\n",ithresh,i,values[i]);
        }
      }
      lprintf("Finished %d\n",ithresh);
    } // end loop over thresh

    mtc->UnsetGTMask(MASKALL);
    lprintf("Finished trigger scan!\n");

  }
  catch(const char* s){
    lprintf("TriggerScan: %s\n",s);
  }

  fclose(file);
  lprintf("****************************************\n");
  return 0;
}

