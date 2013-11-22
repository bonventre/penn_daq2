#ifndef _GENERIC_LINK_H
#define _GENERIC_LINK_H

#define BUSY_CONNECTION_FLAG 2
#define NO_CONNECTION_FLAG 1

#include "NetUtils.h"

class GenericLink{
  public:
    GenericLink(int port);
    GenericLink();
    virtual ~GenericLink();


    virtual void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    static void AcceptCallbackHandler(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx){
      (static_cast<GenericLink*>(ctx))->AcceptCallback(listener,fd,address,socklen);
    };

    virtual void RecvCallback(struct bufferevent *bev){};
    static void RecvCallbackHandler(struct bufferevent *bev, void *arg){
      (static_cast<GenericLink*>(arg))->RecvCallback(bev);
    };

    virtual void SentCallback(struct bufferevent *bev){};
    static void SentCallbackHandler(struct bufferevent *bev, void *arg){
      (static_cast<GenericLink*>(arg))->SentCallback(bev);
    };

    virtual void EventCallback(struct bufferevent *bev, short what){};
    static void EventCallbackHandler(struct bufferevent *bev, short what, void *arg){
      (static_cast<GenericLink*>(arg))->EventCallback(bev,what);
    };

    int IsConnected(){return fConnected;};
    int IsLocked(){return fLock;};
    int SetLock(int lock){fLock = lock;};

  protected:
    int fLock;
    int fConnected;
    struct evconnlistener *fListener;
    int fFD;
    struct bufferevent *fBev;

};

#endif
