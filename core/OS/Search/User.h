#ifndef _SEARCH_USER_H
#define _SEARCH_USER_H
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/User.h>
#include <string>
#include <vector>
#include <map>
#define OPENSPY_USERMGR_URL OPENSPY_WEBSERVICES_URL "/backend/useraccount"
#define OPENSPY_USERMGR_KEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"
namespace OS {
	/*
		Called within the search task thread
	*/
	typedef void (*UserSearchCallback)(bool success, std::vector<OS::User> results, void *extra);


	typedef struct {
		int userid;
		std::string email;
		int partnercode;
		int skip;
		void *extra;
		UserSearchCallback callback;
	} UserSearchRequest;

	

	class UserSearchTask : public Task<UserSearchRequest> {
		public:
			UserSearchTask();
			~UserSearchTask();
			static UserSearchTask *getUserTask();
		private:
			static void PerformSearch(UserSearchRequest request);
			static UserSearchTask *m_task_singleton;
			static void *TaskThread(CThread *thread);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
	};	
}
#endif //_SEARCH_USER_H