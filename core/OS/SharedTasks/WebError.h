#ifndef _WEBERROR_H
#define _WEBERROR_H
#include <jansson.h>
namespace TaskShared {
    enum WebErrorCode {
			WebErrorCode_Success,
            WebErrorCode_AuthInvalidCredentials,
            WebErrorCode_CannotDeleteLastProfile,
            WebErrorCode_NickInUse,
            WebErrorCode_NickInvalid,
            WebErrorCode_NoSuchUser,
            WebErrorCode_UniqueNickInUse,
            WebErrorCode_UniqueNickInvalid,
            WebErrorCode_UserExists,
            WebErrorCode_BackendError, //fallback error, generic comms problem, etc
    };
    typedef struct _WebErrorDetails {
            WebErrorCode response_code;
            int profileid;
            int userid;
    } WebErrorDetails;
    bool Handle_WebError(json_t *json_body, WebErrorDetails &error_info); //in AuthTasks.cpp
}
#endif //_WEBERROR_H