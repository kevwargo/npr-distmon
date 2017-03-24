#ifndef _SOCKET_H_INCLUDED_
#define _SOCKET_H_INCLUDED_

#include <sys/socket.h>

extern int socket_initbind(struct sockaddr_in *saddr);
extern int socket_initconn(struct sockaddr_in *saddr);
extern int socket_parseaddr(const char *s, struct sockaddr_in *saddr);

#endif
