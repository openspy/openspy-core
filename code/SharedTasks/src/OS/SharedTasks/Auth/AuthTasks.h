#ifndef OS_TASKSHARED_AUTH_H
#define OS_TASKSHARED_AUTH_H
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include <curl/curl.h>
#include <jansson.h>
#include <string>

#include <OS/Net/NetPeer.h>

#include "../../WebError.h"
#include "../../tasks.h"

namespace TaskShared {
     enum EAuthType {
        EAuthType_User_EmailPassword,
        EAuthType_Uniquenick_Password,
        EAuthType_MakeAuthTicket,
        EAuthType_NickEmail,
		EAuthType_MakeAuthSession,
		EAuthType_DeleteAuthSession,
		EAuthType_TestPreAuth,
    };

	class AuthData {
		public:
			AuthData() { };
			std::string session_key;
			uint32_t expiresAt;
			uint32_t expiresInSecs;
			std::string response_proof;
			OS::GameData gamedata;
			WebErrorDetails error_details;
	};

    typedef void (*AuthCallback)(bool success, OS::User user, OS::Profile profile, AuthData auth_data, void *extra, INetPeer *peer);

	class AuthRequest {
	public:
		AuthRequest() {
			create_session = true;
			extra = NULL;
			peer = NULL;
			expiresInSecs = 0;
		}
		int type;
		bool create_session;
		int expiresInSecs;

		OS::User user;
		OS::Profile profile;
		AuthCallback callback;
		void *extra;

		INetPeer *peer;
		std::string gamename;

		std::string auth_token;
		std::string auth_token_challenge;
	};

	void AuthReq_InitCurl(void *curl, char *post_data, void *write_data, AuthRequest request, struct curl_slist **out_list);

    bool PerformAuth_Email_Password(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_UniqueNick_Password(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_MakeAuthTicket(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_NickEmail(AuthRequest request, TaskThreadData *thread_data);
	bool PerformAuth_MakeAuthSession(AuthRequest request, TaskThreadData *thread_data);
	bool PerformAuth_DeleteAuthSession(AuthRequest request, TaskThreadData *thread_data);
	bool PerformAuth_TestPreAuth(AuthRequest request, TaskThreadData *thread_data);	

	void AddAuthTaskRequest(AuthRequest request);
}
#endif //OS_TASKSHARED_AUTH_H