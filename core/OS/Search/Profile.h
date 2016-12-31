#ifndef _SEARCH_PROFILE_H
#define _SEARCH_PROFILE_H
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <string>
#include <vector>
#include <map>
//#define OPENSPY_WEBSERVICES_URL "http://10.10.10.10"
#define OPENSPY_PROFILEMGR_URL OPENSPY_WEBSERVICES_URL "/backend/userprofile"
#define OPENSPY_PROFILEMGR_KEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"

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
	typedef void (*ProfileSearchCallback)(EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);

	enum EProfileSearchType {
		EProfileSearch_Profiles,
		EProfileSearch_Buddies,
		EProfileSearch_Buddies_Reverse,
		EProfileSearch_Blocks,
		EProfileSearch_CreateProfile,
		EProfileSearch_DeleteProfile,
		EProfileSearch_UpdateProfile,
	};

	typedef struct {
		std::vector<int> target_profileids; //target search profile ids, only used for buddy reverse searches atm
		std::vector<int> namespaceids;
		OS::Profile profile_search_details;
		OS::User user_search_details;
		int skip;
		void *extra;
		ProfileSearchCallback callback;
		EProfileSearchType type;
	} ProfileSearchRequest;

	

	class ProfileSearchTask : public Task<ProfileSearchRequest> {
		public:
			ProfileSearchTask();
			~ProfileSearchTask();
			static ProfileSearchTask *getProfileTask();
		private:
			static void PerformSearch(ProfileSearchRequest request);
			static ProfileSearchTask *m_task_singleton;
			static void *TaskThread(CThread *thread);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
	};
}
#endif //_SEARCH_USER_H