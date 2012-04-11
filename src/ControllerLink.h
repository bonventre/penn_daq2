#ifndef _CONTROLLER_LINK_H
#define _CONTROLLER__LINKH

#include "NetUtils.h"
#include "GenericLink.h"

class ControllerLink : public GenericLink {
  public:
    ControllerLink();
    ~ControllerLink();

    void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    void RecvCallback(struct bufferevent *bev);
    void SentCallback(struct bufferevent *bev){};
    void EventCallback(struct bufferevent *bev, short what);

    void ProcessCommand(char *input);

  private:
    static int fNumControllers;

};

#endif
