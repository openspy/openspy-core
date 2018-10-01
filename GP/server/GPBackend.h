#ifndef _GP_BACKEND_H
#define _GP_BACKEND_H
#include <GP/server/GPPeer.h>
#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/GPShared.h>

#define GP_BACKEND_REDIS_DB 5
#define BUDDY_ADDREQ_EXPIRETIME 604800
#define GP_STATUS_EXPIRE_TIME 3600

namespace GP {
	class Server;
	class Driver;
}
namespace GPBackend {

	enum EGPRedisRequestType {
		EGPRedisRequestType_BuddyRequest,
		EGPRedisRequestType_AuthorizeAdd,
		EGPRedisRequestType_UpdateStatus,
		EGPRedisRequestType_DelBuddy,
		EGPRedisRequestType_RevokeAuth,
		EGPRedisRequestType_SendLoginEvent,
		EGPRedisRequestType_BuddyMessage,
		EGPRedisRequestType_AddBlock,
		EGPRedisRequestType_DelBlock,
		EGPRedisRequestType_SendGPBuddyStatus,
		EGPRedisRequestType_SendGPBlockStatus
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

	struct sBlockBuddy {
		int from_profileid;
		int to_profileid;
	};

	struct sBuddyMessage {
		char type;
		int to_profileid;
		char message[GP_REASON_LEN+1];
	};

	typedef struct {
		EGPRedisRequestType type;
		union {
			struct sBuddyRequest BuddyRequest;
			struct sAuthorizeAdd AuthorizeAdd;
			struct sDelBuddy DelBuddy;
			struct sBuddyMessage BuddyMessage;
			struct sBlockBuddy BlockMessage;
			
		} uReqData;
		GPShared::GPStatus StatusInfo; //cannot be in union due to OS::Address
		void *extra;
		GP::Peer *peer;
		GPBackendRedisCallback callback;
		
	} GPBackendRedisRequest;

	class GPBackendRedisTask : public OS::Task<GPBackendRedisRequest> {
		public:
			GPBackendRedisTask(int thread_index);
			~GPBackendRedisTask();
			static void MakeBuddyRequest(GP::Peer *peer, int to_profileid, std::string reason);
			static void SetPresenceStatus(int from_profileid, GPShared::GPStatus status, GP::Peer *peer);
			static void MakeAuthorizeBuddyRequest(GP::Peer *peer, int target);
			static void MakeDelBuddyRequest(GP::Peer *peer, int target);
			static void MakeRevokeAuthRequest(GP::Peer *peer, int target);
			static void SendLoginEvent(GP::Peer *peer);
			static void SendMessage(GP::Peer *peer, int to_profileid, char msg_type, const char *message);
			static void MakeBlockRequest(GP::Peer *peer, int block_id);
			static void MakeRemoveBlockRequest(GP::Peer *peer, int block_id);

			static void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);

			void AddDriver(GP::Driver *driver);
			void RemoveDriver(GP::Driver *driver);

		private:
			static void *TaskThread(OS::CThread *thread);
			void Perform_BuddyRequest(GPBackendRedisRequest request);
			void Perform_AuthorizeAdd(GPBackendRedisRequest request);
			void Perform_DelBuddy(GPBackendRedisRequest request);
			void Perform_SetPresenceStatus(GPBackendRedisRequest request);
			void Perform_SendLoginEvent(GPBackendRedisRequest request);
			void Perform_SendBuddyMessage(GPBackendRedisRequest request);
			void Perform_BlockBuddy(GPBackendRedisRequest request);
			void Perform_DelBuddyBlock(GPBackendRedisRequest request);
			void Perform_SendGPBuddyStatus(GPBackendRedisRequest request);

			void load_and_send_gpstatus(GP::Peer *peer, json_t *json);
			void ProfileReq_InitCurl(void *curl, char *post_data, void *write_data, GPBackendRedisRequest request);

			Redis::Connection *mp_redis_connection;
			std::vector<GP::Driver *> m_drivers;
			int m_thread_index;
	};
	#define NUM_PRESENCE_THREADS 8
	#define GP_WAIT_MAX_TIME 1500
	extern OS::TaskPool<GPBackendRedisTask, GPBackendRedisRequest> *m_task_pool;
	void SetupTaskPool(GP::Server *server);
	void ShutdownTaskPool();
	void *setup_redis_async(OS::CThread *thread);
}
#endif //_GP_BACKEND_H