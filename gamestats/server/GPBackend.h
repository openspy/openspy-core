#ifndef _GP_BACKEND_H
#define _GP_BACKEND_H
#include <GP/server/GPPeer.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#include <OS/GPShared.h>

namespace GPBackend {

	enum EPersistRequestType {
		EPersistRequestType_SetUserData,
		EPersistRequestType_GetUserData,
	};

	

	typedef struct {
		
	} PersistBackendResponse;

	typedef void (*PersistBackendCallback)(bool success, PersistBackendResponse response_data, void *extra);



	typedef struct {
		EPersistRequestType type;
		/*
		union {
			struct sBuddyRequest BuddyRequest;
			struct sAuthorizeAdd AuthorizeAdd;
			struct sDelBuddy DelBuddy;
			struct sBuddyMessage BuddyMessage;
			struct sBlockBuddy BlockMessage;
			
		} uReqData;
		*/
		void *extra;
		PersistBackendCallback callback;
		
	} PersistBackendRequest;


	


	class PersistBackendTask : public OS::Task<PersistBackendRequest> {
		public:
			PersistBackendTask();
			~PersistBackendTask();
			static PersistBackendTask *getPersistBackendTask();

		private:
			static PersistBackendTask *m_task_singleton;
			static void *TaskThread(OS::CThread *thread);


			redisContext *mp_redis_connection;
			redisAsyncContext *mp_redis_subscribe_connection;
	};
}
#endif //_GP_BACKEND_H