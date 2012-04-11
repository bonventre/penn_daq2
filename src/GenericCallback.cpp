#include <cstring>

#include "NetUtils.h"
#include "GenericCallback.h"


GenericCallback::GenericCallback(int port)
{
  // set up listener
  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  fListener = evconnlistener_new_bind(evBase,&GenericCallback::AcceptCallbackHandler,this,LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
  if (!fListener){
    printf("Couldn't create listener on port %d\n",port);
  }
}

GenericCallback::~GenericCallback()
{
  evconnlistener_free(fListener);
}

void GenericCallback::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  fFD = fd;
  fBev = bufferevent_socket_new(evBase,fFD,BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(fBev,&GenericCallback::RecvCallbackHandler,&GenericCallback::SentCallbackHandler,&GenericCallback::EventCallbackHandler,this);
  bufferevent_enable(fBev,EV_READ | EV_WRITE);
}


