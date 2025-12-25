#include "SSLTCPDriver.h"
#include "SSLPeer.h"
/*
   Previously this supported SSL2 and even SSL3 for FESL

   MOHPA uses SSL2
   BF2142 uses SSL3

   now BF2142 has been updated to support TLS1.2 so SSL3 is dropped
*/
namespace OS {
     bool g_ssl_init = false;
     int get_ssl_version(const char *string) {
         if(strcmp(string,"TLS1.0") == 0) {
            return TLS1_VERSION;
         }
         # if !defined(OPENSSL_NO_SSL3_METHOD) && !defined(OPENSSL_NO_DEPRECATED_1_1_0)
         else if(strcmp(string,"SSLv3") == 0) {
            return SSL3_VERSION;
         }
         #endif
            else if(strcmp(string,"TLS1.0") == 0) {
            return TLS1_VERSION;
         } else if(strcmp(string,"TLS1.1") == 0) {
            return TLS1_1_VERSION;
         } else if(strcmp(string,"TLS1.2") == 0) {
            return TLS1_2_VERSION;
         } else if(strcmp(string,"TLS1.3") == 0) {
            return TLS1_3_VERSION;
         }
         return -1; //default
     }
     void *GetSSLContext() {
         if(!g_ssl_init) {
            g_ssl_init = true;
            SSL_library_init();
         }
         char temp_env_buffer[256];
         size_t temp_env_sz = sizeof(temp_env_buffer);

         temp_env_sz = sizeof(temp_env_buffer);

         SSL_CTX *ctx = SSL_CTX_new(TLS_method());

         if(uv_os_getenv("OPENSPY_SSL_MIN_VER", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            int version = get_ssl_version(temp_env_buffer);
            if(version != -1) {
               SSL_CTX_set_min_proto_version(ctx, version);
            }
         }

         if(uv_os_getenv("OPENSPY_SSL_MAX_VER", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            int version = get_ssl_version(temp_env_buffer);
            if(version != -1) {
               SSL_CTX_set_max_proto_version(ctx, version);
            }
         }

         if(uv_os_getenv("OPENSPY_SSL_CIPHER_LIST", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            SSL_CTX_set_cipher_list(ctx, temp_env_buffer);
         }

         //SSL_CTX_set_options(ctx, SSL_OP_ALL);

         temp_env_sz = sizeof(temp_env_buffer);
         if(uv_os_getenv("OPENSPY_SSL_CERT", (char *)&temp_env_buffer, &temp_env_sz) == 0) {
            int r = SSL_CTX_use_certificate_chain_file(ctx, temp_env_buffer);
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