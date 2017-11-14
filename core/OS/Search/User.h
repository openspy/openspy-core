#ifndef _SEARCH_USER_H
#define _SEARCH_USER_H
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/User.h>
#include <OS/TaskPool.h>
#include <OS/Net/NetPeer.h>
#include <string>
#include <vector>
#include <map>
#define OPENSPY_USERMGR_URL OPENSPY_WEBSERVICES_URL "/backend/useraccount"
#define OPENSPY_USERMGR_KEY "dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5dGhpc2lzdGhla2V5"
namespace OS {
	/*
		Called within the search task thread
	*/
	enum EUserResponseType {
		EUserResponseType_Success,
		EUserResponseType_GenericError,
		EUserResponseType_EmailInUse,
	};
	typedef void (*UserSearchCallback)(EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer);

	enum EUserRequestType {
		EUserRequestType_Search,
		EUserRequestType_Update,
	};

	typedef struct {
		OS::User search_params;
		EUserRequestType type;
		int skip;
		void *extra;
		UserSearchCallback callback;
		INetPeer *peer;
	} UserSearchRequest;

	

	class UserSearchTask : public Task<UserSearchRequest> {
		public:
			UserSearchTask(int thread_index);
			~UserSearchTask();
		private:
			static void PerformRequest(UserSearchRequest request);
			static void *TaskThread(CThread *thread);
			static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
	};
	extern OS::TaskPool<UserSearchTask, UserSearchRequest> *m_user_search_task_pool;
	void SetupUserSearchTaskPool(int num_tasks);
	void ShutdownUserSearchTaskPool();
}
#endif //_SEARCH_USER_H