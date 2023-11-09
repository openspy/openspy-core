#include "SSLPeer.h"

namespace OS {
    SSLNetPeer::SSLNetPeer(INetDriver* driver, uv_tcp_t *sd) : INetPeer(driver, sd) {

    }
    void SSLNetPeer::InitSSL(void *ssl) {
        printf("SSLNetPeer::InitSSL: %p\n", ssl);
        SSL_CTX *ctx = (SSL_CTX*)ssl;
        mp_ssl = SSL_new(ctx);
        mp_write_bio = BIO_new(BIO_s_mem());
        mp_read_bio = BIO_new(BIO_s_mem());

        BIO_set_nbio(mp_write_bio, 1);
        BIO_set_nbio(mp_read_bio, 1);
        
        SSL_set_bio(mp_ssl, mp_read_bio, mp_write_bio);
        SSL_set_accept_state(mp_ssl);
    }
    void SSLNetPeer::stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        printf("SSLNetPeer::stream_read: %d\n", nread);
        if (nread < 0) {
            if (nread != UV_EOF) {
                OS::LogText(OS::ELogLevel_Info, "[%s] Read error %s\n", m_address.ToString().c_str(), uv_err_name(nread));
            }
            Delete();
        } else if (nread > 0) {
            BIO_write(mp_read_bio, buf->base, nread);
            if (!SSL_is_init_finished(mp_ssl)) {
                printf("init not finished\n");
                int n = SSL_accept(mp_ssl);
                printf("SSL_accept: %d\n", n);
                int status = SSL_get_error(mp_ssl, n);
                printf("ssl error: %d\n", status);
                if(status == SSL_ERROR_WANT_READ) {                    
                    printf("SSL WANT READ %d %d\n", BIO_pending(mp_read_bio), BIO_pending(mp_write_bio));
                }
                ssl_flush();
            }
            if (SSL_is_init_finished(mp_ssl)) {
                printf("SSL INIT FINISHED... TRY READ\n");
                int rc = SSL_read(mp_ssl, buf->base, nread);
                printf("SSL READ: %d\n", rc);
                //SSL_read
                if(rc > 0) {
                    on_stream_read(stream, rc, buf);   
                }
            }
        }

        if (buf->base) {
            free(buf->base);
        }
    }

    void SSLNetPeer::ssl_flush() {
        int ssl_write_sz = BIO_pending(mp_write_bio);
        printf("ssl flush: %p\n", ssl_write_sz);
        if(ssl_write_sz > 0) {
            UVWriteData *write_data = new UVWriteData(1, this);
            uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
            uv_handle_set_data((uv_handle_t*) req, write_data);

            write_data->send_buffers[0] = OS::Buffer(ssl_write_sz);
            BIO_read(mp_write_bio, write_data->send_buffers[0].GetHead(), ssl_write_sz);
            write_data->uv_buffers[0] = uv_buf_init((char *)write_data->send_buffers[0].GetHead(), ssl_write_sz);

            IncRef();
            int r = uv_write(req, (uv_stream_t*)&m_socket, write_data->uv_buffers, 1, write_callback);
            if (r < 0) {
                OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d - %s", getAddress().ToString().c_str(), r, uv_err_name(r));
            }
        }
    }
    void SSLNetPeer::clear_send_buffer() {
        printf("SSLNetPeer::clear_send_buffer");

        uv_mutex_lock(&m_send_mutex);
        if(!m_send_buffer.empty()) {        
            size_t num_buffers = m_send_buffer.size();
           
            for(int i=0;i<num_buffers;i++) {
                OS::Buffer send_buffer = m_send_buffer.front();
                int r = SSL_write(mp_ssl, send_buffer.GetHead(), send_buffer.bytesWritten());
                m_send_buffer.pop();
                DecRef();
            }
        }
        uv_mutex_unlock(&m_send_mutex);
        ssl_flush();
    }
}