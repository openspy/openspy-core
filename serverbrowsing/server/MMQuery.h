#ifndef _MM_QUERY_H
#define _MM_QUERY_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Task.h>
#include <OS/TaskPool.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/Redis.h>
#include <vector>
#include <map>
#include <string>

#include <OS/Timer/HiResTimer.h>

class SBServer;
namespace SB {
	class Driver;
	class Peer;
};


namespace MM {
	class Server {
		public:
			Server() {
				id = 0;
			}
			~Server() {

			}
			OS::Address wan_address;
			OS::Address lan_address;
			OS::GameData game;
			OS::countryRegion region;
			std::map<std::string, std::string> kvFields;

			std::map<int, std::map<std::string, std::string> > kvPlayers;
			std::map<int, std::map<std::string, std::string> > kvTeams;

			std::string key;


			int id;		
	};

	class ServerListQuery {
		public:
			ServerListQuery() {
				first_set = false;
				last_set = false;
			}
			~ServerListQuery() {

			}
			std::vector<std::string> requested_fields;

			std::vector<std::string> captured_basic_fields;
			std::vector<std::string> captured_player_fields;
			std::vector<std::string> captured_team_fields;
			std::vector<Server *> list;
			bool first_set;
			bool last_set;
	};
	
	extern Redis::Connection *mp_redis_connection;

	class sServerListReq {
		public:
			sServerListReq() {
				protocol_version = 0;
				encoding_version = 0;
				game_version = 0;
				send_groups = false;
				push_updates = false;
				no_server_list = false;
				no_list_cache = false;
				send_fields_for_all = false;
				all_keys = false;
			}
			~sServerListReq() {

			}
			uint8_t protocol_version;
			uint8_t encoding_version;
			uint32_t game_version;
		
			std::string filter;
			//const char *field_list;
			std::vector<std::string> field_list;
		
			uint32_t source_ip; //not entire sure what this is for atm
			uint32_t max_results;
		
			bool send_groups;
			//bool send_wan_ip;
			bool push_updates;
			bool no_server_list;
			bool no_list_cache;
			bool send_fields_for_all;
		

			//used after lookup
			OS::GameData m_for_game;
			OS::GameData m_from_game;


			//used before lookup
			std::string m_for_gamename;
			std::string m_from_gamename;

			bool all_keys;
		
	};

	enum EMMQueryRequestType {
		EMMQueryRequestType_GetServers,
		EMMQueryRequestType_GetGroups,
		EMMQueryRequestType_GetServerByKey,
		EMMQueryRequestType_GetServerByIP,
		EMMQueryRequestType_SubmitData,
		EMMQueryRequestType_GetGameInfoByGameName,
		EMMQueryRequestType_GetGameInfoPairByGameName, //get 2 game names in same thread
	};
	class MMQueryRequest {
		public:
			MMQueryRequest() {
				type = EMMQueryRequestType_GetServers;
				peer = NULL;
				driver = NULL;
				extra = NULL;
			}
			~MMQueryRequest() {

			}

			EMMQueryRequestType type;
			//union {
				sServerListReq req;
				//submit data
					OS::Buffer buffer;
					OS::Address from;
					OS::Address to;
//
				OS::Address address; //used for GetServerByIP
				std::string key; //used for GetServerByKey
			//} sQueryData;
			SB::Driver *driver;
			SB::Peer *peer;

			std::string gamenames[2];
			void *extra;
	};

	//
	class MMQueryResponse {
		public:
			MMQueryResponse() {
				peer = NULL;
				extra = NULL;
			};
			~MMQueryResponse() {

			};
			SB::Peer *peer;
			MM::MMQueryRequest request;
			MM::ServerListQuery results;
			void *extra;
	};
	
	class MMQueryTask : public OS::Task<MMQueryRequest> {
		public:
			MMQueryTask(int index);
			~MMQueryTask();

			static void FreeServerListQuery(MM::ServerListQuery *query);

			void AddDriver(SB::Driver *driver);
			void RemoveDriver(SB::Driver *driver);

			static void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);

			void debug_dump();
		private:
			
			static void *TaskThread(OS::CThread *thread);

			void AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, Redis::Connection *redis_ctx, const sServerListReq *req);
			void AppendGroupEntry(const char *entry_name, ServerListQuery *ret, Redis::Connection *redis_ctx, bool all_keys, const MMQueryRequest *request);

			bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key, Redis::Connection *redis_ctx);
			bool FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index, Redis::Connection *redis_ctx);
			bool FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index, Redis::Connection *redis_ctx);

			Server *GetServerByKey(std::string key, Redis::Connection *redis_ctx = NULL, bool include_deleted = false);
			Server *GetServerByIP(OS::Address address, OS::GameData game, Redis::Connection *redis_ctx = NULL);

			ServerListQuery GetServers(const sServerListReq *req, const MMQueryRequest *request = NULL);
			ServerListQuery GetGroups(const sServerListReq *req, const MMQueryRequest *request = NULL);

			void PerformServersQuery(MMQueryRequest request);
			void PerformGroupsQuery(MMQueryRequest request);
			void PerformSubmitData(MMQueryRequest request);
			void PerformGetServerByKey(MMQueryRequest request);
			void PerformGetServerByIP(MMQueryRequest request);
			void PerformGetGameInfoPairByGameName(MMQueryRequest request);
			void PerformGetGameInfoByGameName(MMQueryRequest request);
			
			std::vector<SB::Driver *> m_drivers;
			Redis::Connection *mp_redis_connection;
			time_t m_redis_timeout;
			bool m_thread_awake;
			OS::HiResTimer *mp_timer;
			int m_thread_index;
	};

	#define NUM_MM_QUERY_THREADS 8
	#define MM_WAIT_MAX_TIME 1500
	extern OS::TaskPool<MMQueryTask, MMQueryRequest> *m_task_pool;
	void SetupTaskPool(SBServer *server);
	void ShutdownTaskPool();
	void *setup_redis_async(OS::CThread *thread);

};

#endif //_MM_QUERY_H