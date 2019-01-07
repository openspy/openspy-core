#ifndef OS_TASKS_AUTH_H
#define OS_TASKS_AUTH_H
#include <OS/Task/TaskScheduler.h>
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include <OS/Auth.h>
#include <curl/curl.h>
#include <jansson.h>
#include <string>
#include <OS/Cache/GameCache.h>

#include <OS/SharedTasks/Account/UserTasks.h>

namespace TaskShared {
	enum AuthResponseCode {
		LOGIN_RESPONSE_SUCCESS,
		LOGIN_RESPONSE_SERVERINITFAILED,
		LOGIN_RESPONSE_USER_NOT_FOUND,
		LOGIN_RESPONSE_INVALID_PASSWORD,
		CREATE_RESPONSE_INVALID_EMAIL,
		LOGIN_RESPONSE_INVALID_PROFILE,
		LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED,
		LOGIN_RESPONSE_DB_ERROR,
		LOGIN_RESPONSE_SERVER_ERROR
	};
    enum EAuthType {
        EAuthType_User_EmailPassword,
        EAuthType_Uniquenick_Password,
        EAuthType_MakeAuthTicket,
        EAuthType_NickEmail
    };

	typedef struct {
		std::string session_key;
		std::string response_proof;
		OS::GameData gamedata;
		AuthResponseCode response_code;
	} AuthData;

    typedef void (*AuthCallback)(bool success, OS::User user, OS::Profile profile, AuthData auth_data, void *extra, INetPeer *peer);

	typedef struct {
		int type;
		bool create_session;

		OS::User user;
		OS::Profile profile;
		AuthCallback callback;
		void *extra;

		INetPeer *peer;
		std::string gamename;
	} AuthRequest;

	void AuthReq_InitCurl(void *curl, char *post_data, void *write_data, AuthRequest request);

    bool PerformAuth_Email_Password(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_UniqueNick_Password(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_MakeAuthTicket(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_NickEmail(AuthRequest request, TaskThreadData *thread_data);
    bool PerformAuth_CreateUser_OrProfile(AuthRequest request, TaskThreadData *thread_data);

	void Handle_AuthWebError(TaskShared::AuthData &data, json_t *error_obj);
}
#endif //OS_TASKS_AUTH_H