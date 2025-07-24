/*      "Net Friend" v1.0.0
 *
 *      A friendly network library. Easy and simple!
 *
 *      To use:
 *	    #define NETFRIEND_IMPL
 *	before including the header file, like so
 *	
 *	#include ...
 *	#define NETFRIEND_IMPL
 *	#include "netfriend.h"
 */

#ifndef NETFRIEND_H
#define NETFRIEND_H

#define NF_VERSION 1

#define NF_BACKLOG 8

enum {
	NF_UNIX = 0,
	NF_INET = 1,
};

// Creates listening socket. Takes hostname, port, and socket family enum
// Returns file descriptor of socket, or -1 on error
int nf_listen_conn(const char *, const char *, int);

// Accepts new connection. Takes server file descriptor, and socket family enum
// Returns file descriptor of socket, or -1 on error
int nf_accept_conn(int, int);

#endif // NETFRIEND_H

#ifdef NETFRIEND_IMPL

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

static int nf_set_nonblock(int fd)
{
	int flags, res;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		perror("set_nonblock");
		return -1;
	}
	res = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (res == -1) {
		perror("set_nonblock");
		return -1;
	}

	return 0;
}

static int nf_set_tcpnodelay(int fd)
{
	return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
}

static int nf_bind_conn_unix(const char *sockp)
{
	struct sockaddr_un addr;
	int fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("bind_conn_unix socket()");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockp, sizeof(addr.sun_path) - 1);
	unlink(sockp);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind_conn_unix bind()");
		return -1;
	}

	return fd;
}

static int nf_bind_conn_tcp(const char *host, const char *port)
{
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	};
	struct addrinfo *res, *rp;
	int sfd;

	if (getaddrinfo(host, port, &hints, &res) != 0) {
		perror("bind_conn_tcp getaddrinfo()");
		return -1;
	}

	for (rp = res; rp; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
			perror("bind_conn_tcp setsockopt()");
		if ((bind(sfd, rp->ai_addr, rp->ai_addrlen)) == 0)
			break;

		close(sfd);
	}

	if (!rp) {
		perror("bind_conn_tcp no bind");
		return -1;
	}
	freeaddrinfo(res);

	return sfd;
}

static int nf_bind_conn(const char *host, const char *port, int sock_family)
{
	int fd;
	if (sock_family == NF_UNIX)
		fd = nf_bind_conn_unix(host);
	else
		fd = nf_bind_conn_tcp(host, port);

	return fd;
}

void *nf_getinaddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}


int nf_listen_conn(const char *host, const char *port, int sock_family)
{
	int fd;

	if ((fd = nf_bind_conn(host, port, sock_family)) == -1)
		return -1;
	if ((nf_set_nonblock(fd)) == -1)
		return -1;
	if (sock_family == NF_INET)
		nf_set_tcpnodelay(fd);
	if ((listen(fd, NF_BACKLOG)) == -1) {
		perror("listen_conn listen()");
		return -1;
	}

	return fd;
}

int nf_accept_conn(int fd, int sock_family)
{
	int clientfd;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	if ((clientfd = accept(fd, (struct sockaddr *)&addr, &addrlen)) < 0)
		return -1;
	nf_set_nonblock(clientfd);

	if (sock_family == NF_INET)
		nf_set_tcpnodelay(clientfd);

	char ipbuf[INET_ADDRSTRLEN + 1];
	if (inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf)) == NULL) {
		close(clientfd);
		return -1;
	}

	return clientfd;
}

#endif // NETFRIEND_IMPL
