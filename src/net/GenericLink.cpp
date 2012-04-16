#include <cstring>

#include "Globals.h"

#include "NetUtils.h"
#include "GenericLink.h"


GenericLink::GenericLink(int port)
{
  fConnected = 0;
  fLock = 0;

  // set up listener
  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  fListener = evconnlistener_new_bind(evBase,&GenericLink::AcceptCallbackHandler,this,LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
  if (!fListener){
    printf("Couldn't create listener on port %d\n",port);
    throw 1;
  }
}

GenericLink::GenericLink()
{
  fConnected = 0;
  fLock = 0;
}

GenericLink::~GenericLink()
{
  evconnlistener_free(fListener);
}

void GenericLink::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  fConnected = 1;
  fFD = fd;
  fBev = bufferevent_socket_new(evBase,fFD,BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
  bufferevent_setwatermark(fBev, EV_READ, 0, 0); 
  bufferevent_setcb(fBev,&GenericLink::RecvCallbackHandler,&GenericLink::SentCallbackHandler,&GenericLink::EventCallbackHandler,this);
  bufferevent_enable(fBev,EV_READ | EV_WRITE);
}


