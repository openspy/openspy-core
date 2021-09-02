#ifndef _MM_TASKS_H
#define _MM_TASKS_H
#include <string>

#include <OS/Task/TaskScheduler.h>
#include <OS/Task/ScheduledTask.h>

#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>

#define MM_REDIS_EXPIRE_TIME 500
namespace UT {
    class Driver;
    class Peer;
}

namespace MM {

	class PlayerRecord {
		public:
			std::string name;
	};
	class ServerRecord {
		public:
			int id;
			OS::Address m_address;
			int gameid;
			std::string hostname;
			std::string level;
			std::string game_group;
			int num_players;
			int max_players;

			std::map<std::string, std::string> m_rules;
			std::vector<PlayerRecord> m_players;
	};

	enum UTMasterRequestType {
		UTMasterRequestType_AllocateServerId,
		UTMasterRequestType_ListServers,
		UTMasterRequestType_Heartbeat,
        UTMasterRequestType_DeleteServer
	};

	class MMTaskResponse {
		public:
			int server_id;
			UT::Peer *peer;
			std::vector<ServerRecord> server_records;
	};

	typedef void (*MMTaskResponseCallback)(MMTaskResponse response);

	class UTMasterRequest {
		public:
			UTMasterRequest() {
				type = UTMasterRequestType_ListServers;
				peer = NULL;
				driver = NULL;
			}
			~UTMasterRequest() {

			}

			int type;

			UT::Driver *driver;
			UT::Peer *peer;

			ServerRecord record;

			MMTaskResponseCallback callback;

	};

	TaskScheduler<UTMasterRequest, TaskThreadData> *InitTasks(INetServer *server);

	bool PerformAllocateServerId(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformHeartbeat(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data);


	extern const char *mm_channel_exchange;
    extern const char *mp_pk_name;

}
#endif //_MM_TASKS_H