#ifndef _SSLNETIOINTERFACE_H
#define _SSLNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include "BSDNetIOInterface.h"
#include <vector>

#include <openssl/ssl.h>

namespace SSLNetIOIFace {
    enum ESSL_Type {
            ESSL_None,
            ESSL_SSLv23,
            ESSL_SSLv2,
            ESSL_SSLv3,
            ESSL_TLS10,
            ESSL_TLS11,
            ESSL_TLS12,
    };
	class SSLNetIOInterface;
    class SSL_Socket : public INetIOSocket {
		friend class SSLNetIOInterface;
        public:
            SSL_Socket(SSL_CTX *ctx = NULL, ESSL_Type ssl_version = ESSL_None);
			~SSL_Socket();

            void init(SSL_CTX *ctx, ESSL_Type ssl_version);
        protected:
            SSL *mp_ssl;
			SSL_CTX *mp_ssl_ctx;

            ESSL_Type m_type;
            bool m_ssl_handshake_complete;
            int m_ssl_handshake_attempts;
    };
    class SSLNetIOInterface : public BSDNetIOInterface<SSL_Socket> {
		friend class SSL_Socket;
        public:
            SSLNetIOInterface(ESSL_Type type, std::string privateKey_raw, std::string cert_raw);
            ~SSLNetIOInterface();

            NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer);
            NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer);

			std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket);
        protected:
            bool try_ssl_accept(SSL_Socket *socket);
			SSL_CTX *mp_ssl_ctx;
            ESSL_Type m_ssl_version;
    };
}
#endif //_SSLNETIOINTERFACE_H