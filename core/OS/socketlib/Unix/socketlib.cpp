#include <OS/socketlib/socketlib.h>
#include <fcntl.h>
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
	sktlib_inetaddr		inet_addr;
	sktlib_inetntoa 		inet_ntoa;
	void Init() {
		accept = (fdapi_accept)::accept;
		bind = (fdapi_bind)::bind;
		connect = (fdapi_connect)::connect;
		fcntl = (fdapi_fcntl)::fcntl;
		freeaddrinfo = (fdapi_freeaddrinfo)::freeaddrinfo;
		getaddrinfo = (fdapi_getaddrinfo)::getaddrinfo;
		getsockopt = (fdapi_getsockopt)::getsockopt;
		getpeername = (fdapi_getpeername)::getpeername;
		getsockname = (fdapi_getsockname)::getsockname;
		htonl = (fdapi_htonl)::htonl;
		htons = (fdapi_htons)::htons;
		inet_ntop = (fdapi_inet_ntop)::inet_ntop;
		inet_pton = (fdapi_inet_pton)::inet_pton;
		listen = (fdapi_listen)::listen;


		select = (fdapi_select)::select;
		setsockopt = (fdapi_setsockopt)::setsockopt;
		socket = (fdapi_socket)::socket;
		inet_addr = (sktlib_inetaddr)::inet_addr;
		inet_ntoa	= (sktlib_inetntoa)::inet_ntoa;
	}
	bool wouldBlock() {
		return errno == EWOULDBLOCK;
	}
}
