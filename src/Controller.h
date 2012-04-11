#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "NetUtils.h"
#include "GenericCallback.h"

class Controller : public GenericCallback {
  public:
    Controller();
    ~Controller();

    void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen);
    void RecvCallback(struct bufferevent *bev);
    void SentCallback(struct bufferevent *bev){};
    void EventCallback(struct bufferevent *bev, short what);

    void ProcessCommand(char *input);

  private:
    static int fNumControllers;

};

#endif
