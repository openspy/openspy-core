#ifndef _SSLDRIVER_H
#define _SSLDRIVER_H

#include <OS/Net/drivers/TCPDriver.h>

#include <openssl/ssl.h>

namespace OS {
    void *GetSSLContext(); //retrieves the configured SSL context (void* = SSL_CTX*)
	class SSLTCPDriver : public TCPDriver {
        public:
            SSLTCPDriver(INetServer *server, const char *host, uint16_t port, void *ssl_ctx);
			virtual ~SSLTCPDriver();
			void think();
        protected:
            INetPeer *on_new_connection(uv_stream_t *server, int status);
            SSL_CTX *mp_ctx;
    };
}
#endif // _SSLDRIVER_H