#ifndef _XL3_LINK_H
#define _XL3_LINK_H

#include <queue>
#include <pthread.h>

#include "PacketTypes.h"

#include "NetUtils.h"
#include "GenericLink.h"

class XL3Link : public GenericLink {
  public:
    XL3Link(int crateNum);
    ~XL3Link();

    void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    void RecvCallback(struct bufferevent *bev);
    void SentCallback(struct bufferevent *bev){};
    void EventCallback(struct bufferevent *bev, short what){};
    
    int GetNextPacket(XL3Packet *packet,int waitSeconds=5);
    int GetNextCmdAck(Command *command,int waitSeconds=5);
    int SendPacket(XL3Packet *packet);

  private:
    int fCrateNum;
    std::queue<XL3Packet> fRecvQueue;
    std::queue<Command> fRecvCmdQueue;
    pthread_mutex_t fRecvQueueLock;
    pthread_cond_t fRecvQueueCond;

};

#endif
