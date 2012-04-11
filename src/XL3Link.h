#ifndef _XL3_LINK_H
#define _XL3_LINK_H

#include "NetUtils.h"
#include "GenericLink.h"

class XL3Link : public GenericLink {
  public:
    XL3Link(int crateNum);
    ~XL3Link();

    int fRecvCount;

    void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    void RecvCallback(struct bufferevent *bev);
    void SentCallback(struct bufferevent *bev){};
    void EventCallback(struct bufferevent *bev, short what){};

  private:
    int fCrateNum;
    struct evconnlistener *fListener;
    int fFD;
    struct bufferevent *fBev;

};

#endif
