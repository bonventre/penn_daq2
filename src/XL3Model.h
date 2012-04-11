#ifndef _XL3_MODEL_H
#define _XL3_MODEL_H

#include <stdint.h>

class XL3Model{

  public:
    int RW(uint32_t address, uint32_t data);
    int SendCommand();
};

#endif
