#ifndef _CONTROLLER_LINK_H
#define _CONTROLLER_LINK_H

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

    void GetInput(char *results,int maxLength=0);
    static void *ProcessCommand(void *arg);

  private:
    static int fNumControllers;
    pthread_mutex_t fInputLock;
    pthread_cond_t fInputCond;
    char fInput[10000];

};

#endif
