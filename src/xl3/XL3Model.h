#ifndef _XL3_MODEL_H
#define _XL3_MODEL_H

#include <stdint.h>

#include "PacketTypes.h"
#include "DBTypes.h"

#include "XL3Link.h"

class XL3Model{

  public:
    XL3Model(int crateNum);
    ~XL3Model();

    int RW(uint32_t address, uint32_t data, uint32_t *result);
    int SendCommand(XL3Packet *packet, int withResponse = 1, int timeout = 2);
    int ChangeMode(int mode, uint16_t dataAvailMask);
    int UpdateCrateConfig(uint16_t slotMask);
    int DeselectFECs();
    int GetMultiFCResults(int numCmds, int packetNum, uint32_t *result, int timeout = 2);

    int CheckLock();
    void Lock(){fLink->SetLock(1);};
    void UnLock(){fLink->SetLock(0);};
    uint16_t GetMBID(int slot){return fFECs[slot].mbID;};
    uint16_t GetDBID(int slot, int card){return fFECs[slot].dbID[card];};
    int ConfigureCrate(FECConfiguration *fecs);

  private:
    int fCrateNum;
    XL3Link *fLink;
    FECConfiguration fFECs[16];
};

#endif
