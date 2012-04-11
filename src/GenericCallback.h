#ifndef _GENERIC_CALLBACK_H
#define _GENERIC_CALLBACK_H

#include "NetUtils.h"

class GenericCallback{
  public:
    GenericCallback(int port);
    virtual ~GenericCallback();

    virtual void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    static void AcceptCallbackHandler(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx){
      (static_cast<GenericCallback*>(ctx))->AcceptCallback(listener,fd,address,socklen);
    };

    virtual void RecvCallback(struct bufferevent *bev){};
    static void RecvCallbackHandler(struct bufferevent *bev, void *arg){
      (static_cast<GenericCallback*>(arg))->RecvCallback(bev);
    };

    virtual void SentCallback(struct bufferevent *bev){};
    static void SentCallbackHandler(struct bufferevent *bev, void *arg){
      (static_cast<GenericCallback*>(arg))->SentCallback(bev);
    };

    virtual void EventCallback(struct bufferevent *bev, short what){};
    static void EventCallbackHandler(struct bufferevent *bev, short what, void *arg){
      (static_cast<GenericCallback*>(arg))->EventCallback(bev,what);
    };

  private:
    struct evconnlistener *fListener;
    int fFD;
    struct bufferevent *fBev;
};

#endif
