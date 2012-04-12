#ifndef _NETUTILS_H
#define _NETUTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event2/listener.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#define MAX_XL3_CON     19
#define MAX_CONT_CON    1
#define MAX_VIEW_CON    3
#define MAX_SBC_CON     1

#define SBC_IP          30


int setupListeners();

void signalCallback(evutil_socket_t sig, short events, void *user_data);

#endif
