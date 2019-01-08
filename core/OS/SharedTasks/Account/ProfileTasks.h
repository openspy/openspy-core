#ifndef OS_TASKS_PROFILE_TASKS_H
#define OS_TASKS_PROFILE_TASKS_H
#include <OS/Task/TaskScheduler.h>
#include <OS/OpenSpy.h>
#include <OS/Profile.h>
#include <OS/User.h>
#include <OS/GPShared.h>
namespace TaskShared {
	enum EProfileResponseType {
		EProfileResponseType_Success,
		EProfileResponseType_GenericError,
		EProfileResponseType_BadNick,
		EProfileResponseType_Bad_OldNick,
		EProfileResponseType_UniqueNick_Invalid,
		EProfileResponseType_UniqueNick_InUse,
	};
	typedef void (*ProfileSearchCallback)(EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
	typedef void (*ProfileSearchBuddyCallback)(EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer);


	enum EProfileTaskType {
		EProfileSearch_Profiles,
		EProfileSearch_CreateProfile,
		EProfileSearch_DeleteProfile,
		EProfileSearch_UpdateProfile,
		EProfileSearch_Buddies,
		EProfileSearch_Buddies_Reverse,
		EProfileSearch_Blocks,
	};

	class ProfileRequest {
	public:
		ProfileRequest() { extra = NULL; peer = NULL; callback = NULL; };
		OS::Profile profile_search_details;
		OS::User user_search_details;
		std::vector<int> namespaceids;
		std::vector<int> target_profileids;
		void *extra;
		INetPeer *peer;
		ProfileSearchCallback callback;
		ProfileSearchBuddyCallback buddyCallback;
		int type;
	};

    bool Perform_GetProfiles(ProfileRequest request, TaskThreadData *thread_data);
    bool Perform_CreateProfile(ProfileRequest request, TaskThreadData *thread_data);
    bool Perform_DeleteProfile(ProfileRequest request, TaskThreadData *thread_data);
    bool Perform_UpdateProfile(ProfileRequest request, TaskThreadData *thread_data);
	bool Perform_BuddyRequest(ProfileRequest request, TaskThreadData *thread_data);

	void ProfileReq_InitCurl(void *curl, char *post_data, void *write_data, ProfileRequest request);
	EProfileResponseType Handle_ProfileWebError(ProfileRequest req, json_t *error_obj);
	bool PerformProfileRequest(ProfileRequest request, TaskThreadData *thread_data);
}
#endif //OS_TASKS_PROFILE_TASKS_H