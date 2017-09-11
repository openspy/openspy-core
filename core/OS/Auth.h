#ifndef _OS_AUTH_H
#define _OS_AUTH_H
#include <string>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/TaskPool.h>

#include <OS/Net/NetPeer.h>

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
		CREATE_RESPONE_UNIQUENICK_IN_USE,
		CREATE_RESPONSE_INVALID_NICK,
		CREATE_RESPONSE_INVALID_UNIQUENICK,
	};
	typedef struct {
		std::string session_key;
		std::string hash_proof;
		AuthResponseCode response_code;
	} AuthData;
	typedef void (*AuthCallback)(bool success, User user, Profile profile, AuthData auth_data, void *extra, int operation_id);

	enum EAuthType {
		EAuthType_NickEmail_GPHash,
		EAuthType_NickEmail,
		EAuthType_CreateUser_OrProfile,
		EAuthType_PID_GStats_Sesskey,
	};

	typedef struct {

		int profileid;
		int session_key;

		EAuthType type;
		std::string nick;
		std::string email;
		int partnercode;
		bool create_session;
		std::string password;

		std::string uniquenick;
		int namespaceid;

		//gp hash specific
		std::string server_challenge;
		std::string client_challenge;
		std::string client_response;

		AuthCallback callback;
		void *extra;

		int operation_id;

		INetPeer *peer;
	} AuthRequest;

	class AuthTask : public Task<AuthRequest> {
		public:
			AuthTask();
			~AuthTask();
			static void TryAuthNickEmail_GPHash(std::string nick, std::string email, int partnercode, std::string server_chal, std::string client_chal, std::string client_response, AuthCallback cb, void *extra, int operation_id, INetPeer *peer = NULL);
			static void TryAuthNickEmail(std::string nick, std::string email, int partnercode, std::string pass, bool make_session, AuthCallback cb, void *extra, int operation_id, INetPeer *peer = NULL);
			static void TryCreateUser_OrProfile(std::string nick, std::string uniquenick, int namespaceid, std::string email, int partnercode, std::string password, bool create_session, AuthCallback cb, void *extra, int operation_id, INetPeer *peer = NULL);
			static void TryAuthPID_GStatsSessKey(int profileid, int session_key, std::string response, AuthCallback cb, void *extra, int operation_id, INetPeer *peer = NULL);
		private:
			static AuthTask *m_task_singleton;
			static void *TaskThread(CThread *thread);
			void PerformAuth_NickEMail_GPHash(AuthRequest request);
			void PerformAuth_NickEMail(AuthRequest request);
			void PerformAuth_CreateUser_OrProfile(AuthRequest request);
			void PerformAuth_PID_GSStats_SessKey(AuthRequest request);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
	};
	extern OS::TaskPool<AuthTask, AuthRequest> *m_auth_task_pool;
	void SetupAuthTaskPool(int num_tasks);
	void ShutdownAuthTaskPool();
}
#endif //_OS_AUTH_H