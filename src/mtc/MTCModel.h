#ifndef _MTC_MODEL_H
#define _MTC_MODEL_H

#include <stdint.h>

#include "MTCPacketTypes.h"
#include "MTCRegisters.h"

#include "MTCLink.h"

/* Default setup parameters */

#define DEFAULT_GT_DELAY 150
#define DEFAULT_GT_FINE_DELAY 0
#define DEFAULT_PED_WIDTH 25

#define DEFAULT_LOCKOUT_WIDTH  400               /* in ns */
#define DEFAULT_GT_MASK        EXT8_PULSE_ASYNC
#define DEFAULT_PED_CRATE_MASK MASKALL
#define DEFAULT_GT_CRATE_MASK  MASKALL 

/* Dac load stuff */

#define MTCA_DAC_SLOPE                  (2048.0/5000.0)
#define MTCA_DAC_OFFSET                 ( + 2048 )

/* Masks */

#define MASKALL 0xFFFFFFFFUL
#define MASKNUN 0x00000000UL

#define MSK_TUB   MSK_CRATE21

/* Xilinx load stuff */

#define MAX_DATA_SIZE                   150000 // max number of allowed bits



class MTCModel{

  public:
    MTCModel();
    ~MTCModel();

    int CloseConnection(){fLink->CloseConnection();};
    int Connect(){fLink->Connect();};
    int RegRead(uint32_t address, uint32_t *data);
    int RegWrite(uint32_t address, uint32_t data);
    int SendCommand(SBCPacket *packet, int withResponse = 1, int timeout = 2);

    int CheckLock();
    void Lock(){fLink->SetLock(1);};
    void UnLock(){fLink->SetLock(0);};

    int CheckQueue(int empty){return fLink->CheckQueue(empty);};

    int GetGTCount(uint32_t *count);
    float SetGTDelay(float gtdel);
    int SoftGT();
    int MultiSoftGT(int number);

    int SetupPedestals(float pulser_freq, uint32_t ped_width, 	uint32_t coarse_delay,
        uint32_t fine_delay, uint32_t ped_crate_mask, uint32_t gt_crate_mask);
    int XilinxLoad();
    static char* GetXilinxData(long *howManyBits);
    int LoadMTCADacsByCounts(uint16_t *raw_dacs);
    int LoadMTCADacs(float *voltages);
    int SetLockoutWidth(uint16_t width);
    int SetGTCounter(uint32_t count);
    int SetPrescale(uint16_t scale);
    int SetPulserFrequency(float freq);
    int SetPedestalWidth(uint16_t width);
    int SetCoarseDelay(uint16_t delay);
    float SetFineDelay(float delay);
    void ResetMemory();

    void EnablePedestal();
    void DisablePedestal();
    void EnablePulser();
    void DisablePulser();
    void UnsetGTMask(uint32_t raw_trig_types);
    void SetGTMask(uint32_t raw_trig_types);
    void UnsetPedCrateMask(uint32_t crates);
    void SetPedCrateMask(uint32_t crates);
    void UnsetGTCrateMask(uint32_t crates);
    void SetGTCrateMask(uint32_t crates);

  private:
    MTCLink *fLink;
};

#endif
