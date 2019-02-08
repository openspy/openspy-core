#ifndef OS_TASKSHARED_AUTH_H
#define OS_TASKSHARED_AUTH_H
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include <curl/curl.h>
#include <jansson.h>
#include <string>
#include <OS/Cache/GameCache.h>

namespace TaskShared {
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
		WebErrorDetails error_details;
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
}
#endif //OS_TASKSHARED_AUTH_H