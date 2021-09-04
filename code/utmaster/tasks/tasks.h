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
			int ping;
			int score;
			int stats_id;
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

			std::vector<std::string> m_mutators;

			bool isStandardMutator(std::string mutator) {
				return mutator.compare("DMMutator") == 0;
			}

			bool isStandardServer() {
				if(m_mutators.size() > 0) {
					std::vector<std::string>::iterator it = m_mutators.begin();
					while(it != m_mutators.end()) {
						std::string s = *it;
						if(!isStandardMutator(s)) return false;
						it++;
					}
				}
				return true;
			}
	};

	enum UTMasterRequestType {
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

	class FilterProperties {
		public:
			std::string field;
			std::string property;
			bool is_negate;
	};

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
			//std::map<std::string, std::string> m_filter_items;
			//std::map<std::string, std::string> m_filter_negate_items;
			std::vector<FilterProperties> m_filters;

			MMTaskResponseCallback callback;

	};

	TaskScheduler<UTMasterRequest, TaskThreadData> *InitTasks(INetServer *server);

	bool PerformAllocateServerId(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformHeartbeat(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformListServers(UTMasterRequest request, TaskThreadData *thread_data);
	bool PerformDeleteServer(UTMasterRequest request, TaskThreadData *thread_data);

	int GetServerID(TaskThreadData *thread_data);
	bool isServerDeleted(TaskThreadData *thread_data, std::string server_key);
	std::string GetServerKey_FromIPMap(UTMasterRequest request, TaskThreadData *thread_data, OS::GameData game_info);


	extern const char *mm_channel_exchange;
    extern const char *mp_pk_name;
	extern const char *mm_server_event_routingkey;

}
#endif //_MM_TASKS_H