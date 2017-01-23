#include "math.h"

#include "XL3PacketTypes.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"
#include "Globals.h"
#include "Json.h"

#include "DB.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "RunPedestals.h"

int RunPedestals(int crateMask, uint32_t *slotMasks, uint32_t channelMask, float frequency, int gtDelay, int pedWidth, int setupCrates, int setupMTC)
{
  lprintf("*** Setting up a Pedestal Run **********\n\n");

  int errors;

  if (setupCrates){
    lprintf("Crate Settings:\n");
    lprintf("--------------------------------\n");
    lprintf("Crate Mask:     0x%05x\n",crateMask);
    for (int i=0;i<MAX_XL3_CON;i++)
      if ((0x1<<i) & crateMask)
        lprintf("Crate %d: Slot Mask:    0x%04x\n",i,slotMasks[i]); 
    lprintf("Channel Mask:     0x%08x\n\n",channelMask);
  }
  if (setupMTC){
    lprintf("MTC Settings:\n");
    lprintf("--------------------------------\n");
    lprintf("GT delay (ns):      %3hu\n",gtDelay);
    lprintf("Pedestal Width (ns):      %2d\n",pedWidth);
    lprintf("Pulser Frequency (Hz):      %3.0f\n\n",frequency);
  }

  try {

  // Put all xl3s in init mode for now and reset all the FECs
  if (setupCrates){
    for (int i=0;i<MAX_XL3_CON;i++){
      if ((0x1<<i) & crateMask){
        lprintf("Preparing crate %d.\n",i);
        xl3s[i]->ChangeMode(INIT_MODE,0x0);
        for (int slot=0;slot<16;slot++){
          if ((0x1<<slot) & slotMasks[i]){
            uint32_t select_reg = FEC_SEL*slot;
            uint32_t result;
            xl3s[i]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0xFFFFFFFF,&result);
            xl3s[i]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x2,&result);
            xl3s[i]->RW(GENERAL_CSR_R + select_reg + WRITE_REG,0x0,&result);
            xl3s[i]->RW(CMOS_CHIP_DISABLE_R + select_reg + WRITE_REG,0x0,&result);
          }
        }
        xl3s[i]->DeselectFECs();
        errors = xl3s[i]->LoadCrateAddr(slotMasks[i]);
        errors += xl3s[i]->SetCratePedestals(slotMasks[i],channelMask);
        xl3s[i]->DeselectFECs();

        if (errors){
          lprintf("Error setting up crate %d, exiting\n",i);
          return -1;
        }
      }
    }
  }

  // now set up pedestals on mtc
  if (setupMTC){
    lprintf("Preparing mtcd\n");
    errors = mtc->SetupPedestals(frequency,pedWidth,gtDelay,DEFAULT_GT_FINE_DELAY,
        crateMask | MSK_TUB, crateMask | MSK_TUB);
    if (errors){
      lprintf("run_pedestals: Error setting up MTC. Exiting\n");
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);
      return -1;
    }
  }

  // now enable the readout on the xl3s
  if (setupCrates)
    for (int i=0;i<MAX_XL3_CON;i++)
      if ((0x1<<i) & crateMask)
        xl3s[i]->ChangeMode(NORMAL_MODE,slotMasks[i]);

  }
  catch(const char* s){
    lprintf("RunPedestals: %s\n",s);
  }

  lprintf("****************************************\n\n");
  return 0;
}

int RunPedestalsEnd(int crateMask, int setupCrates, int setupMTC)
{
  lprintf("*** Turning off Pedestal Run ***********\n\n");

  try {

    // put all the crates back into init mode so they stop reading out
    // and turn off the pedestals
    if (setupCrates)
      for (int i=0;i<MAX_XL3_CON;i++)
        if ((0x1<<i) & crateMask){
          lprintf("Stopping crate %d\n",i);
          xl3s[i]->ChangeMode(INIT_MODE,0x0);
          xl3s[i]->SetCratePedestals(0xFFFF,0x0);
        }

    // turn off the pulser and unmask all the crates
    if (setupMTC){
      lprintf("Stopping mtcd\n");
      mtc->DisablePulser();
      mtc->DisablePedestal();
      mtc->UnsetPedCrateMask(MASKALL);
      mtc->UnsetGTCrateMask(MASKALL);
    }

  }
  catch(const char* s){
    lprintf("RunPedestals: %s\n",s);
  }

  lprintf("****************************************\n\n");
  return 0;
}
