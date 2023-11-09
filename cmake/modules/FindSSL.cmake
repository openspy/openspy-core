find_package(OpenSSL)

IF(OPENSSL_FOUND)
    SET(SSL_LIBS "${OPENSSL_LIBRARIES}")
    SET(SSL_OS_INC "${CMAKE_CURRENT_SOURCE_DIR}/../core/SSL/OpenSSL")
    file (GLOB SSL_SRCS "SSL/OpenSSL/SSL/*.cpp")
    
ELSE()
    file (GLOB SSL_SRCS "SSL/SSLNull/SSL/*.cpp" "SSL/SSLNull/SSL/*.c")
    SET(SSL_OS_INC "${CMAKE_CURRENT_SOURCE_DIR}/../core/SSL/SSLNull")
ENDIF()

message(SSL_SRCS="${SSL_SRCS}")
message(SSL_LIBS="${SSL_LIBS}")
