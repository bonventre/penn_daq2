#ifndef _XL3_H
#define _XL3_H

#include "NetUtils.h"

class XL3{
  public:
    XL3(int crateNum);
    ~XL3();

    static void AcceptCallbackHandler(evutil_socket_t fd, short events, void *ctx);
    void AcceptCallback(evutil_socket_t fd, short events);

    static void RecvCallbackHandler(evutil_socket_t fd, short events, void *ctx){};
    void RecvCallbackHandler(evutil_socket_t fd, short events){};

    static void SentCallbackHandler(evutil_socket_t fd, short events, void *ctx){};
    void SentCallbackHandler(evutil_socket_t fd, short events){};

  private:
    int fCrateNum;
    struct evconnlistener *fListener;

};

#endif
