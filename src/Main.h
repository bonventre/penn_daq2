#ifndef _MAIN_H
#define _MAIN_H

#include <pthread.h>

#include "XL3Link.h"
#include "NetUtils.h"

extern XL3Link *xl3[MAX_XL3_CON];
extern pthread_mutex_t startTestLock;

#endif
