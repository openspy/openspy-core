find_package(OpenSSL)

IF(OPENSSL_FOUND)
    SET(SSL_LIBS "${OPENSSL_LIBRARIES}")
    include_directories (${OPENSSL_INCLUDE_DIR})
    file (GLOB SSL_SRCS "SSL/OpenSSL/SSL/*.cpp")
    
ELSE()
    file (GLOB SSL_SRCS "SSL/SSLNull/SSL/*.cpp" "SSL/SSLNull/SSL/*.c")
ENDIF()

message(SSL_SRCS="${SSL_SRCS}")
