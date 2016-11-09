#include "socketlib.h"

namespace Socket {
	fdapi_accept		 accept;
	fdapi_bind           bind;
	fdapi_connect        connect;
	fdapi_fcntl          fcntl;
	fdapi_fstat          fdapi_fstat64;
	fdapi_freeaddrinfo   freeaddrinfo;
	fdapi_getaddrinfo    getaddrinfo;
	fdapi_getsockopt     getsockopt;
	fdapi_getpeername    getpeername;
	fdapi_getsockname    getsockname;
	fdapi_htonl          htonl;
	fdapi_htons          htons;
	fdapi_inet_ntop      inet_ntop;
	fdapi_inet_pton      inet_pton;
	fdapi_listen         listen;
	fdapi_ntohl          ntohl;
	fdapi_ntohs          ntohs;
	fdapi_select         select;
	fdapi_setsockopt     setsockopt;
	fdapi_socket         socket;
	void Init() {
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);

		HMODULE mdl = GetModuleHandle("ws2_32.dll");
		accept = (fdapi_accept)GetProcAddress(mdl, "accept");
		bind = (fdapi_bind)GetProcAddress(mdl, "bind");
		connect = (fdapi_connect)GetProcAddress(mdl, "connect");
		fcntl = (fdapi_fcntl)GetProcAddress(mdl, "fcntl");
		freeaddrinfo = (fdapi_freeaddrinfo)GetProcAddress(mdl, "freeaddrinfo");
		getaddrinfo = (fdapi_getaddrinfo)GetProcAddress(mdl, "getaddrinfo");
		getsockopt = (fdapi_getsockopt)GetProcAddress(mdl, "getsockopt");
		getpeername = (fdapi_getpeername)GetProcAddress(mdl, "getpeername");
		getsockname = (fdapi_getsockname)GetProcAddress(mdl, "getsockname");
		htonl = (fdapi_htonl)GetProcAddress(mdl, "htonl");
		htons = (fdapi_htons)GetProcAddress(mdl, "htons");
		inet_ntop = (fdapi_inet_ntop)GetProcAddress(mdl, "inet_ntop");
		inet_pton = (fdapi_inet_pton)GetProcAddress(mdl, "inet_pton");
		listen = (fdapi_listen)GetProcAddress(mdl, "listen");


		select = (fdapi_select)GetProcAddress(mdl, "select");
		setsockopt = (fdapi_setsockopt)GetProcAddress(mdl, "setsockopt");
		socket = (fdapi_socket)GetProcAddress(mdl, "socket");
	}
}