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

#define NF_BACKLOG 16

// Creates listening socket. Takes hostname, port, and socket family enum
// Returns file descriptor of socket, or -1 on error
int nf_listen_conn(const char *host, const char *port);

// Accepts new connection. Takes server file descriptor, and socket family enum
// Returns file descriptor of socket, or -1 on error
int nf_accept_conn(int fd);

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
		perror("nf_set_nonblock fcntl F_GETFL");
		return -1;
	}
	res = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (res == -1) {
		perror("nf_set_nonblock fcntl F_SETFL");
		return -1;
	}

	return 0;
}

static int nf_bind_conn(const char *host, const char *port)
{
	struct addrinfo hints, *res, *rp;
	int sfd;
	int yes = 1;
	
	memset(&hints, 0, sizeof(hints));
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE,
	};

	if (getaddrinfo(host, port, &hints, &res) != 0) {
		perror("nf_bind_conn() getaddrinfo");
		return -1;
	}

	for (rp = res; rp; rp = rp->ai_next) {
		if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			perror("nf_bind_conn socket");
			continue;
		}
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("nf_bind_conn setsockopt");
			return -1;
		}
		if ((bind(sfd, rp->ai_addr, rp->ai_addrlen)) == 0) {
			close(sfd);
			perror("nf_bind_conn bind");
			continue;
		}

		break;
	}

	freeaddrinfo(res);
	
	if (!rp) {
		fprintf(stderr, "nf_bind_conn failed to bind\n");
		return -1;
	}

	return sfd;
}

void *nf_getinaddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sa)->sin_addr);

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}


int nf_listen_conn(const char *host, const char *port)
{
	int fd;

	if ((fd = nf_bind_conn(host, port)) == -1)
		return -1;
	if ((nf_set_nonblock(fd)) == -1) {
		return -1;
	}
	if ((listen(fd, NF_BACKLOG)) == -1) {
		perror("nf_listen_conn listen");
		return -1;
	}

	return fd;
}

int nf_accept_conn(int fd)
{
	int clientfd;
	struct sockaddr_storage client_addr;
	socklen_t addrlen = sizeof(client_addr);
	char ipbuf[INET_ADDRSTRLEN + 1];

	if ((clientfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
		perror("nf_accept_conn accept");
		return -1;
	}
	
	if ((nf_set_nonblock(clientfd)) == -1) {
		close(clientfd);
		return -1;
	}

	if (inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf)) == NULL) {
		close(clientfd);
		return -1;
	}

	return clientfd;
}

#endif // NETFRIEND_IMPL
