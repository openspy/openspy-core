#ifndef _OS_SSL_H
#define _OS_SSL_H
namespace OS {
    enum ESSL_Type {
            ESSL_None,
            ESSL_SSLv23,
            ESSL_SSLv2,
            ESSL_SSLv3,
            ESSL_TLS10,
            ESSL_TLS11,
            ESSL_TLS12,
    };
}
#endif // _OS_SSL_H