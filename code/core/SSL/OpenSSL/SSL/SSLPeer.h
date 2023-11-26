#ifndef _SSLPEER_H
#define _SSLPEER_H
#include <OS/Net/NetPeer.h>
#include <openssl/ssl.h>
namespace OS {
    class SSLNetPeer : public INetPeer {
        public:
            SSLNetPeer(INetDriver* driver, uv_tcp_t *sd);
            virtual ~SSLNetPeer();
            void InitSSL(void *ssl);
            void stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
            void clear_send_buffer();
            void ssl_flush();
        private:
            SSL *mp_ssl;
            BIO *mp_write_bio;
            BIO *mp_read_bio;
    };
}
#endif //_SSLPEER_H