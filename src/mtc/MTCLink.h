#ifndef _MTC_LINK_H
#define _MTC_LINK_H

#include <queue>
#include <pthread.h>

#include "MTCPacketTypes.h"

#include "NetUtils.h"
#include "GenericLink.h"

class MTCLink : public GenericLink {
  public:
    MTCLink();
    ~MTCLink();

    int CloseConnection();
    int Connect();
    void RecvCallback(struct bufferevent *bev);
    void SentCallback(struct bufferevent *bev){};
    void EventCallback(struct bufferevent *bev, short what){};
    
    int GetNextPacket(SBCPacket *packet,int waitSeconds=5);
    int SendPacket(SBCPacket *packet);

  private:
    std::queue<SBCPacket> fRecvQueue;
    pthread_mutex_t fRecvQueueLock;
    pthread_cond_t fRecvQueueCond;

};

#endif
