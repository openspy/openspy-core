#ifndef _MM_TASKS_H
#define _MM_TASKS_H
#include <string>
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <hiredis/hiredis.h>
#include <uv.h>

#define NN_REDIS_EXPIRE_TIME 500
namespace QR {
    class Server;
	class Driver;
}
namespace MM {
	class TaskThreadData {
		public:
			redisContext *mp_redis_connection;
	};

	typedef struct {
		OS::GameData m_game;
		OS::Address  m_address;
		std::map<std::string, std::string> m_keys;
		std::map<std::string, std::vector<std::string> > m_player_keys;
		std::map<std::string, std::vector<std::string> > m_team_keys;

		int groupid;
		int id;
	} ServerInfo;

	void Shutdown();

	enum EMMPushRequestType {
		EMMPushRequestType_GetGameInfoByGameName,
		EMMPushRequestType_Heartbeat,
		EMMPushRequestType_Heartbeat_ClearExistingKeys,
		EMMPushRequestType_ValidateServer,
		EMMPushRequestType_Keepalive,
		EMMPushRequestType_ClientMessageAck

	};
	class MMTaskResponse {
		public:
			MMTaskResponse() {
				error_message = NULL;
				v2_instance_key = 0;
				driver = NULL;
			}
			QR::Driver *driver;
			OS::Address from_address;
			OS::Address query_address;
			uint32_t v2_instance_key;
			OS::GameData game_data;
			OS::Buffer comms_buffer;
			
			const char *error_message;
			std::string challenge;
	};

	typedef void (*MMTaskResponseCallback)(MMTaskResponse response);
	class MMPushRequest {
	public:
		MMPushRequest() {
			type = EMMPushRequestType_GetGameInfoByGameName;
		}
		~MMPushRequest() { }
		
		ServerInfo server;
		std::string gamename;

		OS::Address from_address;
		uint32_t v2_instance_key;
		QR::Driver *driver;
		int version;
		int type;

		MMTaskResponseCallback callback;
	};

	class MMWorkData {
		public:
			MMWorkData() : request(), response() {

			}
			MMPushRequest request;
			MMTaskResponse response;

	};

	void InitTasks();

	void PerformUVWorkRequest(uv_work_t *req);
	void PerformUVWorkRequestCleanup(uv_work_t *req, int status);

	void sendAMQPMessage(const char *exchange, const char *routingkey, const char *messagebody);
    
    bool PerformGetGameInfo(MMPushRequest request, TaskThreadData *thread_data);
	bool PerformKeepalive(MMPushRequest request, TaskThreadData *thread_data);
	bool PerformHeartbeat(MMPushRequest request, TaskThreadData *thread_data);
	bool PerformValidate(MMPushRequest request, TaskThreadData *thread_data);
	bool PerformClientMessageAck(MMPushRequest request, TaskThreadData *thread_data);

    //server update functions
    bool PerformDeleteMissingKeysAndUpdateChanged(MMPushRequest request, TaskThreadData *thread_data);

    bool Handle_QRMessage(std::string message);

	//shared functions
	int TryFindServerID(TaskThreadData *thread_data, OS::Address address);
	int GetServerID(TaskThreadData *thread_data);
	bool isTeamString(const char *string);
	bool isPlayerString(std::string key, std::string &variable_name, int &player_id);
	OS::Address GetQueryAddressForServer(TaskThreadData *thread_data, std::string server_key);
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key, bool ignoreChallengeExists = false);
	void selectQRRedisDB(TaskThreadData *thread_data);

	void WriteServerData(TaskThreadData *thread_data, std::string server_key, ServerInfo server, uint32_t instance_key, OS::Address from_address);
	void SetServerDeleted(TaskThreadData *thread_data, std::string server_key, bool deleted);
	void SetServerInitialInfo(TaskThreadData *thread_data, OS::Address driver_address, std::string server_key, OS::GameData game_info, std::string challenge_response, OS::Address address, int id, OS::Address from_address);
	void ClearServerCustKeys(TaskThreadData *thread_data, std::string server_key);
	void WriteLastHeartbeatTime(TaskThreadData *thread_data, std::string server_key, OS::Address address, uint32_t instance_key, OS::Address from_address);

	std::string GetNewServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data, std::string gamename, int &server_id);
	std::string GetServerKey_FromRequest(MMPushRequest request, TaskThreadData *thread_data);

	std::string GetServerKeyBy_InstanceKey_Address(TaskThreadData *thread_data, uint32_t instance_key, OS::Address address);
	int GetNumHeartbeats(TaskThreadData *thread_data, std::string server_key);

	extern const char *mm_channel_exchange;

	extern const char *mm_client_message_routingkey;
	extern const char *mm_server_event_routingkey;
	extern const char *mm_server_client_acks_routingkey;

    extern const char *mp_pk_name;

	//#define NUM_MM_PUSH_THREADS 8
	#define MM_WAIT_MAX_TIME 1500
	#define MM_PUSH_EXPIRE_TIME 600
	#define MM_IGNORE_TIME_SECS 60 //number of time in seconds where another heartbeat update will be blocked
	#define MM_QRV1_NUM_BURST_REQS 3
}
#endif //_MM_TASKS_H