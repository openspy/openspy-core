#ifndef _GP_BACKEND_H
#define _GP_BACKEND_H
#include <GP/server/GPPeer.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
namespace GPBackend {

	enum EGPRedisRequestType {
		EGPRedisRequestType_BuddyRequest,
		EGPRedisRequestType_AuthorizeAdd,
		EGPRedisRequestType_UpdateStatus,
		EGPRedisRequestType_DelBuddy,
		EGPRedisRequestType_RevokeAuth,
	};

	

	typedef struct {
		
	} GPBackendRedisResponse;

	typedef void (*GPBackendRedisCallback)(bool success, GPBackendRedisResponse response_data, void *extra);

	struct sBuddyRequest {
		int from_profileid;
		int to_profileid;
		char reason[GP_REASON_LEN + 1];
	};

	struct sAuthorizeAdd {
		int from_profileid;
		int to_profileid;
	};

	struct sDelBuddy {
		int from_profileid;
		int to_profileid;
	};

	typedef struct {
		EGPRedisRequestType type;
		union {
			struct sBuddyRequest BuddyRequest;
			struct sAuthorizeAdd AuthorizeAdd;
			struct sDelBuddy DelBuddy;
			GPStatus StatusInfo;
		} uReqData;
		void *extra;
		GPBackendRedisCallback callback;
		
	} GPBackendRedisRequest;


	


	class GPBackendRedisTask : public OS::Task<GPBackendRedisRequest> {
		public:
			GPBackendRedisTask();
			~GPBackendRedisTask();
			static GPBackendRedisTask *getGPBackendRedisTask();
			static void MakeBuddyRequest(int from_profileid, int to_profileid, const char *reason);
			static void SetPresenceStatus(int from_profileid, GPStatus status, GP::Peer *peer);
			static void MakeAuthorizeBuddyRequest(int adding_target, int adding_source);
			static void MakeDelBuddyRequest(int adding_target, int adding_source);
			static void MakeRevokeAuthRequest(int adding_target, int adding_source);
		private:
			static GPBackendRedisTask *m_task_singleton;
			static void *TaskThread(OS::CThread *thread);
			void Perform_BuddyRequest(struct sBuddyRequest request);
			void Perform_AuthorizeAdd(struct sAuthorizeAdd request);
			void Perform_DelBuddy(struct sDelBuddy request, bool send_revoke);
			void Perform_SetPresenceStatus(GPStatus status, void *extra);

			redisContext *mp_redis_connection;
			redisAsyncContext *mp_redis_subscribe_connection;
	};
}
#endif //_GP_BACKEND_H