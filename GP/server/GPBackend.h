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
		EGPRedisRequestType_SendLoginEvent,
		EGPRedisRequestType_BuddyMessage,
		EGPRedisRequestType_AddBlock,
		EGPRedisRequestType_DelBlock,
		EGPRedisRequestType_SendGPBuddyStatus,
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
		GPStatus StatusInfo; //cannot be in union due to OS::Address
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
			static void SendLoginEvent(GP::Peer *peer);
			static void SendMessage(GP::Peer *peer, int to_profileid, char msg_type, const char *message);
			static void MakeBlockRequest(int from_profileid, int block_id);
			static void MakeRemoveBlockRequest(int from_profileid, int block_id);
		private:
			static GPBackendRedisTask *m_task_singleton;
			static void *TaskThread(OS::CThread *thread);
			void Perform_BuddyRequest(struct sBuddyRequest request);
			void Perform_AuthorizeAdd(struct sAuthorizeAdd request);
			void Perform_DelBuddy(struct sDelBuddy request, bool send_revoke);
			void Perform_SetPresenceStatus(GPStatus status, void *extra);
			void Perform_SendLoginEvent(GP::Peer *peer);
			void Perform_SendBuddyMessage(GP::Peer *peer, struct sBuddyMessage msg);
			void Perform_BlockBuddy(struct sBlockBuddy msg);
			void Perform_DelBuddyBlock(struct sBlockBuddy msg);
			void Perform_SendGPBuddyStatus(GP::Peer *peer);

			void load_and_send_gpstatus(GP::Peer *peer, json_t *json);

			redisContext *mp_redis_connection;
			redisAsyncContext *mp_redis_subscribe_connection;
	};
}
#endif //_GP_BACKEND_H