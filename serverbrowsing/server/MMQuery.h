#ifndef _MM_QUERY_H
#define _MM_QUERY_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/TaskPool.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>
#include <OS/Redis.h>
#include <vector>
#include <map>
#include <string>

class SBServer;
namespace SB {
	class Driver;
	class Peer;
};


namespace MM {
	typedef struct {
		OS::Address wan_address;
		OS::Address lan_address;
		OS::GameData game;
		OS::countryRegion region;
		std::map<std::string, std::string> kvFields;

		std::map<int, std::map<std::string, std::string> > kvPlayers;
		std::map<int, std::map<std::string, std::string> > kvTeams;

		std::string key;


		int id;		
	} Server;

	struct ServerListQuery {
		std::vector<std::string> requested_fields;

		std::vector<std::string> captured_basic_fields;
		std::vector<std::string> captured_player_fields;
		std::vector<std::string> captured_team_fields;
		std::vector<Server *> list;
		bool first_set;
		bool last_set;
	};
	
	extern Redis::Connection *mp_redis_connection;

	struct sServerListReq {
		uint8_t protocol_version;
		uint8_t encoding_version;
		uint32_t game_version;
		
		std::string filter;
		//const char *field_list;
		std::vector<std::string> field_list;
		
		uint32_t source_ip; //not entire sure what this is for atm
		uint32_t max_results;
		
		bool send_groups;
		bool send_wan_ip;
		bool push_updates;
		bool no_server_list;
		

		//used after lookup
		OS::GameData m_for_game;
		OS::GameData m_from_game;


		//used before lookup
		std::string m_for_gamename;
		std::string m_from_gamename;

		bool all_keys;
		
	};

	struct _MMQueryRequest;
	enum EMMQueryRequestType {
		EMMQueryRequestType_GetServers,
		EMMQueryRequestType_GetGroups,
		EMMQueryRequestType_GetServerByKey,
		EMMQueryRequestType_GetServerByIP,
		EMMQueryRequestType_SubmitData,
		EMMQueryRequestType_GetGameInfoByGameName,
		EMMQueryRequestType_GetGameInfoPairByGameName, //get 2 game names in same thread
	};
	typedef struct _MMQueryRequest {
		EMMQueryRequestType type;
		//union {
			sServerListReq req;
			struct {
				std::string base64;
				struct sockaddr_in from;
				struct sockaddr_in to;
				OS::GameData game;
			} SubmitData;
			OS::Address address; //used for GetServerByIP
			std::string key; //used for GetServerByKey
		//} sQueryData;
		SB::Driver *driver;
		SB::Peer *peer;

		std::string gamenames[2];
		void *extra;
	} MMQueryRequest;

	//
	typedef struct _MMQueryResponse {
		SB::Peer *peer;
		struct MM::_MMQueryRequest request;
		struct MM::ServerListQuery results;
		void *extra;
	} MMQueryResponse;
	
	class MMQueryTask : public OS::Task<MMQueryRequest> {
		public:
			MMQueryTask();
			~MMQueryTask();
			static void Shutdown();

			static void FreeServerListQuery(struct MM::ServerListQuery *query);

			void AddDriver(SB::Driver *driver);
			void RemoveDriver(SB::Driver *driver);

			static void onRedisMessage(Redis::Connection *c, Redis::Response reply, void *privdata);
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
	};

	#define NUM_MM_QUERY_THREADS 8
	extern OS::TaskPool<MMQueryTask, MMQueryRequest> *m_task_pool;
	void SetupTaskPool(SBServer *server);
	void *setup_redis_async(OS::CThread *thread);

};

#endif //_MM_QUERY_H