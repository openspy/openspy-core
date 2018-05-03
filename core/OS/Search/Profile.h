#ifndef _SEARCH_PROFILE_H
#define _SEARCH_PROFILE_H
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/TaskPool.h>
#include <OS/Net/NetPeer.h>
#include <string>
#include <vector>
#include <map>

#define OPENSPY_PROFILEMGR_URL (std::string(OS::g_webServicesURL) + "/backend/userprofile").c_str()

namespace OS {

	/*
		Called within the search task thread
	*/
	enum EProfileResponseType {
		EProfileResponseType_Success,
		EProfileResponseType_GenericError,
		EProfileResponseType_BadNick,
		EProfileResponseType_Bad_OldNick,
		EProfileResponseType_UniqueNick_Invalid,
		EProfileResponseType_UniqueNick_InUse,
	};
	typedef void (*ProfileSearchCallback)(EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

	enum EProfileSearchType {
		EProfileSearch_Profiles,
		EProfileSearch_Buddies,
		EProfileSearch_Buddies_Reverse,
		EProfileSearch_Blocks,
		EProfileSearch_CreateProfile,
		EProfileSearch_DeleteProfile,
		EProfileSearch_UpdateProfile,
	};

	class ProfileSearchRequest {
	public:
		ProfileSearchRequest() { skip = 0; extra = NULL; peer = NULL; callback = NULL; };
		std::vector<int> target_profileids; //target search profile ids, only used for buddy reverse searches atm
		std::vector<int> namespaceids;
		OS::Profile profile_search_details;
		OS::User user_search_details;
		int skip;
		void *extra;
		INetPeer *peer;
		ProfileSearchCallback callback;
		EProfileSearchType type;
	} ;

	

	class ProfileSearchTask : public Task<ProfileSearchRequest> {
		public:
			ProfileSearchTask(int thread_index);
			~ProfileSearchTask();
		private:
			static EProfileResponseType Handle_ProfileWebError(ProfileSearchRequest req, json_t *error_obj);
			static void PerformSearch(ProfileSearchRequest request);
			static void *TaskThread(CThread *thread);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
			static void ProfileReq_InitCurl(void *curl, char *post_data, void *write_data, ProfileSearchRequest request);
	};
	extern OS::TaskPool<ProfileSearchTask, ProfileSearchRequest> *m_profile_search_task_pool;
	OS::TaskPool<ProfileSearchTask, ProfileSearchRequest> *GetProfileTaskPool();
	void SetupProfileTaskPool(int num_tasks);
	void ShutdownProfileTaskPool();
}
#endif //_SEARCH_USER_H