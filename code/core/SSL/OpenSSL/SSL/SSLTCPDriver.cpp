#include "SSLTCPDriver.h"
#include "SSLPeer.h"
namespace OS {
     bool g_ssl_init = false;
     void *GetSSLContext() {
         if(!g_ssl_init) {
            g_ssl_init = true;
            SSL_library_init();
         }
         char temp_env_buffer[256];
         size_t temp_env_sz = sizeof(temp_env_buffer);

         temp_env_sz = sizeof(temp_env_buffer);

         SSL_CTX *ctx = NULL;

         if(uv_os_getenv("OPENSPY_SSL_VERSION", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            if(strcmp(temp_env_buffer,"SSLv23") == 0) {
               ctx = SSL_CTX_new(SSLv23_method());
            } 
            # if !defined(OPENSSL_NO_SSL3_METHOD) && !defined(OPENSSL_NO_DEPRECATED_1_1_0)
            else if(strcmp(temp_env_buffer,"SSLv2") == 0) {
               ctx = SSL_CTX_new(SSLv2_method());
            } else if(strcmp(temp_env_buffer,"SSLv3") == 0) {
               ctx = SSL_CTX_new(SSLv3_method());
            }
            #endif
             else if(strcmp(temp_env_buffer,"TLS1.0") == 0) {
               ctx = SSL_CTX_new(TLSv1_method());
            } else if(strcmp(temp_env_buffer,"TLS1.1") == 0) {
               ctx = SSL_CTX_new(TLSv1_1_method());
            } else if(strcmp(temp_env_buffer,"TLS1.2") == 0) {
               ctx = SSL_CTX_new(TLSv1_2_method());
            } else if(strcmp(temp_env_buffer,"TLS") == 0) { //shorter len... put last
               ctx = SSL_CTX_new(TLS_method());
            } else {
               OS::LogText(OS::ELogLevel_Error, "Unknown SSL version");
               return NULL;
            }
         } else {
            OS::LogText(OS::ELogLevel_Error, "Missing SSL version");
            return NULL;
         }

         SSL_CTX_set_cipher_list(ctx, "ALL");
         SSL_CTX_set_options(ctx, SSL_OP_ALL);

         temp_env_sz = sizeof(temp_env_buffer);
         if(uv_os_getenv("OPENSPY_SSL_CERT", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            int r = SSL_CTX_use_certificate_chain_file(ctx, temp_env_buffer, SSL_FILETYPE_PEM);
            if(r != 1) {
               OS::LogText(OS::ELogLevel_Error, "Error loading certificate file: %d", r);
               SSL_CTX_free(ctx);
               return NULL;
            }
         } else {
            OS::LogText(OS::ELogLevel_Error, "Missing SSL certificate environment variable");
            SSL_CTX_free(ctx);
            return NULL;
         }

         temp_env_sz = sizeof(temp_env_buffer);
         if(uv_os_getenv("OPENSPY_SSL_KEY", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            int r = SSL_CTX_use_PrivateKey_file(ctx, temp_env_buffer, SSL_FILETYPE_PEM);
            if(r != 1) {
               OS::LogText(OS::ELogLevel_Error, "Error loading private key file: %d", r);
               SSL_CTX_free(ctx);
               return NULL;
            }
         } else {
            OS::LogText(OS::ELogLevel_Error, "Missing SSL Private key environment variable");
            SSL_CTX_free(ctx);
            return NULL;
         }
         return ctx;
     }

     SSLTCPDriver::SSLTCPDriver(INetServer *server, const char *host, uint16_t port, void *ssl_ctx) : TCPDriver(server, host, port) {
        mp_ctx = (SSL_CTX *)ssl_ctx;
     }
     SSLTCPDriver::~SSLTCPDriver() {
      if(mp_ctx) {
         SSL_CTX_free(mp_ctx);
      }
     }
     void SSLTCPDriver::think() {
        TCPDriver::think();
     }
     INetPeer *SSLTCPDriver::on_new_connection(uv_stream_t *server, int status) {
        SSLNetPeer *peer = (SSLNetPeer*)TCPDriver::on_new_connection(server, status);
        peer->InitSSL(mp_ctx);
        return peer;
     }
}