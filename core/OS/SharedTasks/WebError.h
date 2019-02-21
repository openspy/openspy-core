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
			WebErrorCode_EmailInvalid,
			WebErrorCode_BadCdKey,
			WebErrorCode_CdKeyAlreadyTaken,
			WebErrorCode_CdKeyAlreadySet,
            WebErrorCode_BackendError, //fallback error, generic comms problem, etc
    };
    class WebErrorDetails {
	public:
			WebErrorDetails() { response_code = WebErrorCode_BackendError; userid = 0; profileid = 0; }
            WebErrorCode response_code;
            int profileid;
            int userid;
    } ;
    bool Handle_WebError(json_t *json_body, WebErrorDetails &error_info); //in AuthTasks.cpp
}
#endif //_WEBERROR_H