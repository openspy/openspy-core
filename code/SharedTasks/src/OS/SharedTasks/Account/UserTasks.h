#ifndef OS_TASKS_USER_TASKS_H
#define OS_TASKS_USER_TASKS_H
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include "../../tasks.h"
#include "../../WebError.h"
namespace TaskShared {
	typedef void (*UserSearchCallback)(TaskShared::WebErrorDetails error_details, std::vector<OS::User> results, void *extra, INetPeer *peer);
	
	enum EUserRequestType {
		EUserRequestType_Create,
		EUserRequestType_Search,
		EUserRequestType_Update,
		EUserRequestType_Register,
	};

	typedef struct {
		OS::GameData gamedata;
		OS::User user;
		OS::Profile profile;
		WebErrorDetails error_details;
	} UserRegisterData;

	typedef void(*RegisterCallback)(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer);
	
	typedef struct {
		OS::User search_params;
		OS::Profile profile_params; //used for register only
		int type;
		int skip;
		void *extra;
		UserSearchCallback callback;
		RegisterCallback registerCallback;
		INetPeer *peer;
		std::string gamename;
	} UserRequest;

	bool PerformUserRequest(UserRequest request, TaskThreadData *thread_data);
	bool PerformUserRegisterRequest(UserRequest request, TaskThreadData *thread_data);

	void AddUserTaskRequest(UserRequest request);

}
#endif //_USER_TASKS_H