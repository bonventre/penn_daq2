#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <pthread.h>

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "ControllerLink.h"


extern XL3Model *xl3s[MAX_XL3_CON];
extern XL3Link *xl3Connections[MAX_XL3_CON];
extern ControllerLink *contConnection;
extern pthread_mutex_t startTestLock;
extern struct event_base *evBase;

int LockConnections(int sbc, uint32_t xl3s);
int UnlockConnections(int sbc, uint32_t xl3s);

#endif
