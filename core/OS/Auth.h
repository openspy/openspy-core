#ifndef _OS_AUTH_H
#define _OS_AUTH_H
#include <string>

#include <OS/User.h>
#include <OS/Profile.h>

//#define OPENSPY_WEBSERVICES_URL "http://10.10.10.10"
#define OPENSPY_AUTH_URL OPENSPY_WEBSERVICES_URL "/backend/auth"
#define OPENSPY_AUTH_KEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"
namespace OS {
	enum AuthResponseCode {
		LOGIN_RESPONSE_SUCCESS,
		LOGIN_RESPONSE_SERVERINITFAILED,
		LOGIN_RESPONSE_USER_NOT_FOUND,
		LOGIN_RESPONSE_INVALID_PASSWORD,
		LOGIN_RESPONSE_INVALID_PROFILE,
		LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED,
		LOGIN_RESPONSE_DB_ERROR,
		LOGIN_RESPONSE_SERVER_ERROR,
	};
	typedef struct {
		const char *session_key;
		const char *hash_proof;
		AuthResponseCode response_code;

	} AuthData;
	typedef void (*AuthCallback)(bool success, User user, Profile profile, AuthData auth_data, void *extra);
	void TryAuthNickEmail_GPHash(std::string nick, std::string email, int partnercode, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra = NULL);
}
#endif //_OS_AUTH_H