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
	typedef void (*ProfileSearchCallback)(bool success, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra);

	typedef struct {
		int profileid;
		std::string email;
		std::string nick;
		std::string uniquenick;
		int partnercode;
		std::vector<int> namespaceids;
		std::string firstname;
		std::string lastname;
		int icquin;
		int skip;

		void *extra;
		ProfileSearchCallback callback;
	} ProfileSearchRequest;

	

	class ProfileSearchTask : public Task<ProfileSearchRequest> {
		public:
			ProfileSearchTask();
			~ProfileSearchTask();
			void Process();
			static ProfileSearchTask *getProfileTask();
		private:
			static void PerformSearch(ProfileSearchRequest request);
			static ProfileSearchTask *m_task_singleton;
			static void *TaskThread(CThread *thread);
	};
}
#endif //_SEARCH_USER_H