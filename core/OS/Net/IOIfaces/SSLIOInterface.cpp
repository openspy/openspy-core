#include "SSLIOInterface.h"
#define MAX_SSL_ACCEPT_ATTEMPTS 5

namespace SSLNetIOIFace {
    SSL_Socket::SSL_Socket(SSL_CTX *ctx, ESSL_Type ssl_version) {
		m_ssl_handshake_complete = false;
		m_ssl_handshake_attempts = 0;
        if(ctx) {
			init(ctx, ssl_version);
        } else {
            mp_ssl_ctx = NULL;
            mp_ssl = NULL;
        }
    }
    SSL_Socket::~SSL_Socket() {
        if(mp_ssl) {
            SSL_free(mp_ssl);
        }
    }
	void SSL_Socket::init(SSL_CTX *ctx, ESSL_Type ssl_version) {
		mp_ssl_ctx = ctx;
		mp_ssl = SSL_new(mp_ssl_ctx);
		SSL_set_fd(mp_ssl, sd);
	}
    SSLNetIOInterface::SSLNetIOInterface(ESSL_Type type, std::string privateKey_raw, std::string cert_raw) {
		m_ssl_version = type;
        switch (m_ssl_version) {
            case ESSL_SSLv23:
                mp_ssl_ctx = SSL_CTX_new(SSLv23_method());
                break;
            # ifndef OPENSSL_NO_SSL2_METHOD
            case ESSL_SSLv2:
                mp_ssl_ctx = SSL_CTX_new(SSLv2_method());
                break;
            #endif
            # ifndef OPENSSL_NO_SSL3_METHOD
            case ESSL_SSLv3:
                mp_ssl_ctx = SSL_CTX_new(SSLv3_method());
                break;
            #endif
            case ESSL_TLS10:
                mp_ssl_ctx = SSL_CTX_new(TLSv1_method());
                break;
            case ESSL_TLS11:
                mp_ssl_ctx = SSL_CTX_new(TLSv1_1_method());
                break;
            case ESSL_TLS12:
                mp_ssl_ctx = SSL_CTX_new(TLSv1_2_method());
                break;
            case ESSL_None:
            default:
                mp_ssl_ctx = NULL;
                break;
        }
		
        if(mp_ssl_ctx) {
            SSL_CTX_set_cipher_list(mp_ssl_ctx, "ALL");
            SSL_CTX_set_options(mp_ssl_ctx, SSL_OP_ALL);

			if (!SSL_CTX_use_certificate_file(mp_ssl_ctx, cert_raw.c_str(), SSL_FILETYPE_PEM) ||
				!SSL_CTX_use_PrivateKey_file(mp_ssl_ctx, privateKey_raw.c_str(), SSL_FILETYPE_PEM)) {
				fprintf(stderr, "\nError: problems with the loading of the certificate in memory\n");
				exit(1);
			}
            SSL_CTX_set_verify_depth(mp_ssl_ctx, 1);
        }
    }
    SSLNetIOInterface::~SSLNetIOInterface() {
		if(mp_ssl_ctx)
			SSL_CTX_free(mp_ssl_ctx);
    }

    NetIOCommResp SSLNetIOInterface::streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
        SSL_Socket *ssl_socket = (SSL_Socket *)socket;
        NetIOCommResp resp;
        if(!ssl_socket->m_ssl_handshake_complete && ssl_socket->mp_ssl_ctx) {
            if(!try_ssl_accept(ssl_socket)) {
                if(ssl_socket->m_ssl_handshake_attempts >= MAX_SSL_ACCEPT_ATTEMPTS) {
                    resp.disconnect_flag = true;
                    resp.error_flag = true;
                }
                return resp;
            }
        }
        if(ssl_socket->mp_ssl) {
            char recvbuf[1492];
            buffer.reset();
            int c = SSL_read(ssl_socket->mp_ssl, recvbuf, sizeof(recvbuf));
            int e = SSL_get_error(ssl_socket->mp_ssl, c);
            if(c > 0) {
                resp.comm_len = c;
                resp.packet_count++;
                buffer.WriteBuffer(recvbuf, c);
            } else if(e != SSL_ERROR_WANT_READ) {
                resp.disconnect_flag = true;
                resp.error_flag = true;
            }           
        } else {
            return BSDNetIOInterface::streamRecv(socket, buffer);
        }
        buffer.reset();
        return resp;
    }
    NetIOCommResp SSLNetIOInterface::streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
        SSL_Socket *ssl_socket = (SSL_Socket *)socket;
        NetIOCommResp resp;
        if(ssl_socket->mp_ssl) {
            int c = SSL_write(ssl_socket->mp_ssl, buffer.GetHead(), buffer.size());
			int e = SSL_get_error(ssl_socket->mp_ssl, c);
			if (e == SSL_ERROR_WANT_WRITE) {
				m_stream_send_queue[socket].push_back(buffer);
				return resp;
			}
            if(c <= 0) {
                resp.error_flag = true;
                resp.disconnect_flag = true;
                resp.comm_len = 0;
            } else {
                resp.comm_len = c;
            }
        } else {
            return BSDNetIOInterface::streamSend(socket, buffer);
        }
        
        return resp;
    }
    bool SSLNetIOInterface::try_ssl_accept(SSL_Socket *socket) {
        int c = SSL_accept(socket->mp_ssl);
        if(c != 1) {
            //print error
            socket->m_ssl_handshake_attempts++;
        } else {
            socket->m_ssl_handshake_complete = true;
        }
        return socket->m_ssl_handshake_complete;
    }
	std::vector<INetIOSocket *> SSLNetIOInterface::TCPAccept(INetIOSocket *socket) {
		std::vector<INetIOSocket *> sockets = BSDNetIOInterface::TCPAccept(socket);

		std::vector<INetIOSocket *>::iterator it = sockets.begin();
		while (it != sockets.end()) {
			SSL_Socket *sock = (SSL_Socket *)*it;
			sock->init(mp_ssl_ctx, m_ssl_version);
			it++;
		}
		return sockets;
	}
}