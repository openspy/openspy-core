#ifndef OS_TASKS_USER_TASKS_H
#define OS_TASKS_USER_TASKS_H
#include <OS/Task/TaskScheduler.h>
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
namespace TaskShared {
	enum EUserResponseType {
		EUserResponseType_Success,
		EUserResponseType_GenericError,
		EUserResponseType_UserExists,
		EUserResponseType_Profile_UniqueNickInUse,
		EUserResponseType_Profile_InvalidNick,
		EUserResponseType_Profile_InvalidUniqueNick,
	};
	typedef void (*UserSearchCallback)(EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer);
	
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
		EUserResponseType user_response_code;
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

}
#endif //_USER_TASKS_H