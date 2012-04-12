#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <pthread.h>
#include "stdint.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "ControllerLink.h"


extern XL3Model *xl3s[MAX_XL3_CON];
extern ControllerLink *contConnection;
extern pthread_mutex_t startTestLock;
extern struct event_base *evBase;

int LockConnections(int sbc, uint32_t xl3List);
int UnlockConnections(int sbc, uint32_t xl3List);

void SwapLongBlock(void* p, int32_t n);
void SwapShortBlock(void* p, int32_t n);


#endif
