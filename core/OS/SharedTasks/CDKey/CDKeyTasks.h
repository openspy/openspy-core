#ifndef OS_TASKSHARED_CDKEY_H
#define OS_TASKSHARED_CDKEY_H
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
     enum ECDKeyType {
        ECDKeyType_AssociateToProfile,
        ECDKeyType_GetProfileByCDKey,
        ECDKeyType_TestCDKeyValid,
    };

	typedef struct {
		std::string cdkey;
		OS::Profile profile;
		OS::User user;
		OS::GameData gamedata;
		WebErrorDetails error_details;
	} CDKeyData;

    typedef void (*CDKeyCallback)(CDKeyData auth_data, void *extra, INetPeer *peer);

	class CDKeyRequest {
		public:
			CDKeyRequest() {
				extra = NULL;
				peer = NULL;
				callback = NULL;
			}
			int type;

			OS::Profile profile;
			CDKeyCallback callback;
			void *extra;

			INetPeer *peer;
			std::string gamename;
			int gameid;

			std::string cdkey;
			std::string cdkeyHash;
	};

	void CDKeyReq_InitCurl(void *curl, char *post_data, void *write_data, CDKeyRequest request);

    bool PerformCDKey_AssociateToProfile(CDKeyRequest request, TaskThreadData *thread_data);
    bool PerformCDKey_GetProfileByCDKey(CDKeyRequest request, TaskThreadData *thread_data);
    bool PerformCDKey_TestCDKeyValid(CDKeyRequest request, TaskThreadData *thread_data);
}
#endif //OS_TASKSHARED_AUTH_H