#ifndef _MTC_MODEL_H
#define _MTC_MODEL_H

#include <stdint.h>

#include "MTCPacketTypes.h"

#include "MTCLink.h"

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

  private:
    MTCLink *fLink;
};

#endif
