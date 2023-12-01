find_package(OpenSSL)

IF(OPENSSL_FOUND)
    SET(SSL_LIBS "${OPENSSL_LIBRARIES}")
    SET(SSL_OS_INC "${CMAKE_CURRENT_SOURCE_DIR}/../core/SSL/OpenSSL")
    SET(SSL_INCS "${OPENSSL_INCLUDE_DIR}")
    file (GLOB SSL_SRCS "SSL/OpenSSL/SSL/*.cpp")
    
ELSE()
    message( FATAL_ERROR "OpenSSL not found!" )
ENDIF()

message(SSL_SRCS="${SSL_SRCS}")
message(SSL_LIBS="${SSL_LIBS}")
message(SSL_INCS="${SSL_INCS}")
