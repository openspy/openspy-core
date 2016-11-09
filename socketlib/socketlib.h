#ifndef _SOCKETLIB_H
#define _SOCKETLIB_H
#include "../main.h"
#include <stdint.h>
namespace Socket {
	// API prototypes must match the unix implementation
	typedef int(*fdapi_socket)(int af, int type, int protocol);
	typedef int(*fdapi_setsockopt)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	typedef int(*fdapi_fcntl)(int fd, int cmd, int flags);
	typedef int(*fdapi_getsockopt)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
	typedef int(*fdapi_connect)(int sockfd, const struct sockaddr *addr, size_t addrlen);
	typedef int(*fdapi_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

	typedef int(*fdapi_listen)(int sockfd, int backlog);
	typedef int(*fdapi_bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	typedef u_short(*fdapi_htons)(u_short hostshort);
	typedef u_long(*fdapi_htonl)(u_long hostlong);
	typedef u_short(*fdapi_ntohs)(u_short netshort);
	typedef int(*fdapi_getpeername)(int sockfd, struct sockaddr *addr, socklen_t * addrlen);
	typedef int(*fdapi_getsockname)(int sockfd, struct sockaddr* addrsock, int* addrlen);
	typedef void(*fdapi_freeaddrinfo)(struct addrinfo *ai);
	typedef int(*fdapi_getaddrinfo)(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
	typedef const char* (*fdapi_inet_ntop)(int af, const void *src, char *dst, size_t size);
	typedef int(*fdapi_inet_pton)(int af, const char * src, void *dst);
	typedef int(*fdapi_select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	typedef u_int(*fdapi_ntohl)(u_int netlong);
	typedef int(*fdapi_fstat)(int fd, struct __stat64 *buffer);

	typedef BOOL fnWSIOCP_CloseSocketStateRFD(int rfd);

	extern fdapi_accept			accept;
	extern fdapi_bind           bind;
	extern fdapi_connect        connect;
	extern fdapi_fcntl          fcntl;
	extern fdapi_fstat          fdapi_fstat64;
	extern fdapi_freeaddrinfo   freeaddrinfo;
	extern fdapi_getaddrinfo    getaddrinfo;
	extern fdapi_getsockopt     getsockopt;
	extern fdapi_getpeername    getpeername;
	extern fdapi_getsockname    getsockname;
	extern fdapi_htonl          htonl;
	extern fdapi_htons          htons;
	extern fdapi_inet_ntop      inet_ntop;
	extern fdapi_inet_pton      inet_pton;
	extern fdapi_listen         listen;
	extern fdapi_ntohl          ntohl;
	extern fdapi_ntohs          ntohs;
	extern fdapi_select         select;
	extern fdapi_setsockopt     setsockopt;
	extern fdapi_socket         socket;

	void Init();
}
#endif //_SOCKETLIB_H