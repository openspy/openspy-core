#ifndef OS_TASKS_PROFILE_TASKS_H
#define OS_TASKS_PROFILE_TASKS_H
#include <OS/OpenSpy.h>
#include "../../tasks.h"
#include "../../WebError.h"
#include "../../Profile.h"
#include "../../User.h"
#include "../../GPShared.h"
namespace TaskShared {
	typedef void (*ProfileSearchCallback)(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
	typedef void (*ProfileSearchBuddyCallback)(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer);


	enum EProfileTaskType {
		EProfileSearch_Profiles,
		EProfileSearch_CreateProfile,
		EProfileSearch_DeleteProfile,
		EProfileSearch_UpdateProfile,
		EProfileSearch_Buddies,
		EProfileSearch_Buddies_Reverse,
		EProfileSearch_Blocks,
		EProfileSearch_SuggestUniquenick,
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
	bool Perform_BuddyRequest(ProfileRequest request, TaskThreadData *thread_data);
	bool Perform_SuggestUniquenick(ProfileRequest request, TaskThreadData *thread_data);

	void ProfileReq_InitCurl(void *curl, char *post_data, void *write_data, ProfileRequest request, struct curl_slist **out_list);
	bool PerformProfileRequest(ProfileRequest request, TaskThreadData *thread_data);

	void PerformProfileUVWorkRequest(uv_work_t *req);
	void PerformProfileUVWorkRequestCleanup(uv_work_t *req, int status);

	void AddProfileTaskRequest(ProfileRequest request);
}
#endif //OS_TASKS_PROFILE_TASKS_H