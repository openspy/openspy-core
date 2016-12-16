#ifndef _OS_AUTH_H
#define _OS_AUTH_H
#include <string>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
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

	enum EAuthType {
		EAuthType_NickEmail_GPHash,
	};

	typedef struct {
		EAuthType type;
		std::string nick;
		std::string email;
		int partnercode;

		//gp hash specific
		std::string server_challenge;
		std::string client_challenge;
		std::string client_response;

		AuthCallback callback;
		void *extra;
	} AuthRequest;

	class AuthTask : public Task<AuthRequest> {
		public:
			AuthTask();
			~AuthTask();
			static AuthTask *getAuthTask();
			static void TryAuthNickEmail_GPHash(std::string nick, std::string email, int partnercode, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra);
		private:
			static void PerformSearch(AuthRequest request);
			static AuthTask *m_task_singleton;
			static void *TaskThread(CThread *thread);
			void PerformAuth_NickEMail_GPHash(AuthRequest request);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
	};
}
#endif //_OS_AUTH_H