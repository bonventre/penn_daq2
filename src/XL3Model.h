#ifndef _XL3_MODEL_H
#define _XL3_MODEL_H

#include <stdint.h>

#include "XL3Link.h"

class XL3Model{

  public:
    XL3Model(int crateNum);
    ~XL3Model();

    int RW(uint32_t address, uint32_t data, uint32_t *result);
    int SendCommand(XL3Packet *packet, int withResponse = 1);

    int CheckLock();
    void Lock(){fLink->SetLock(1);};
    void UnLock(){fLink->SetLock(0);};

  private:
    int fCrateNum;
    XL3Link *fLink;
};

#endif
