#ifndef _XL3_MODEL_H
#define _XL3_MODEL_H

#include <stdint.h>

#include "XL3PacketTypes.h"
#include "DBTypes.h"

#include "XL3Link.h"

#define FEC_CSR_CRATE_OFFSET 11
#define MAX_FEC_COMMANDS 60000

#define INIT_MODE 0x1
#define NORMAL_MODE 0x2

class XL3Model{

  public:
    XL3Model(int crateNum);
    ~XL3Model();

    int SendCommand(XL3Packet *packet, int withResponse = 1, int timeout = 2);
    int GetMultiFCResults(int numCmds, int packetNum, uint32_t *result, int timeout = 2);
    int GetCaldTestResults(uint16_t *point_buf, uint16_t *adc_buf);

    int RW(uint32_t address, uint32_t data, uint32_t *result);
    int ChangeMode(int mode, uint16_t dataAvailMask);
    int UpdateCrateConfig(uint16_t slotMask);
    int DeselectFECs();
    int GetCmosTotalCount(uint16_t slotMask, uint32_t totalCount[][32]);
    int LoadsDac(uint32_t dacNum, uint32_t dacValue, int slotNum);
    int MultiLoadsDac(int numDacs, uint32_t *dacNums, uint32_t *dacValues, uint32_t *slotNums);
    int32_t ReadOutBundles(int slotNum, uint32_t *pmtBuffer, int limit, int checkLimit=0);
    int SetCratePedestals(uint16_t slotMask, uint32_t pattern);
    int LoadCrateAddr(uint16_t slotMask);
    int SetupChargeInjection(uint32_t slotMask, uint32_t chanMask, uint32_t dacValue);

    int CheckLock();
    void Lock(){fLink->SetLock(1);};
    void UnLock(){fLink->SetLock(0);};
    uint16_t GetMBID(int slot){return fFECs[slot].mbID;};
    uint16_t GetDBID(int slot, int card){return fFECs[slot].dbID[card];};
    int ConfigureCrate(FECConfiguration *fecs);
    int GetLastCommandNum(){return fCommandNum-1;};

  private:
    int fCrateNum;
    XL3Link *fLink;
    FECConfiguration fFECs[16];
    int fCommandNum;
};

struct channel_params{
  int hi_balanced;
  int low_balanced;
  short int test_status;
  short int low_gain_balance;
  short int high_gain_balance;
};

struct cell {
  int16_t per_cell;
  int cellno;
  double qlxbar, qlxrms;
  double qhlbar, qhlrms;
  double qhsbar, qhsrms;
  double tacbar, tacrms;
};

struct pedestal {
  int16_t channelnumber;
  int16_t per_channel;
  struct cell thiscell[16];
};



#endif
