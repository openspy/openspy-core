#include "SSLIOInterface.h"
#define MAX_SSL_ACCEPT_ATTEMPTS 5

namespace SSLNetIOIFace {
    SSL_Socket::SSL_Socket(OS::ESSL_Type ssl_version) {

    }
    SSL_Socket::~SSL_Socket() {

    }
    SSLNetIOInterface::SSLNetIOInterface(OS::ESSL_Type type, std::string privateKey_raw, std::string cert_raw) {
		m_ssl_version = type;
    }
    SSLNetIOInterface::~SSLNetIOInterface() {
    }

    NetIOCommResp SSLNetIOInterface::streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
        NetIOCommResp resp;

        return resp;
    }
    NetIOCommResp SSLNetIOInterface::streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
        SSL_Socket *ssl_socket = (SSL_Socket *)socket;
        NetIOCommResp resp;

        return resp;
    }
    bool SSLNetIOInterface::try_ssl_accept(SSL_Socket *socket) {
        return true;
    }
	std::vector<INetIOSocket *> SSLNetIOInterface::TCPAccept(INetIOSocket *socket) {
		std::vector<INetIOSocket *> sockets = BSDNetIOInterface::TCPAccept(socket);


		return sockets;
	}
}