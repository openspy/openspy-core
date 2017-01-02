#ifndef _MM_QUERY_H
#define _MM_QUERY_H
#include "main.h"

#include <OS/OpenSpy.h>
#include <OS/Task.h>
#include <OS/Thread.h>
#include <OS/Mutex.h>

#include <vector>
#include <map>
#include <string>

#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#define _WINSOCK2API_
#include <stdint.h>
#include <hiredis/adapters/libevent.h>
#undef _WINSOCK2API_

namespace SB {
	class Driver;
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
	};
	

	
	
	extern redisContext *mp_redis_connection;

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
		
		OS::GameData m_for_game;
		OS::GameData m_from_game;

		bool all_keys;



	};

	struct _MMQueryRequest;
	typedef void (*mpMMQueryCB)(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results,void *extra);
	enum EMMQueryRequestType {
		EMMQueryRequestType_GetServers,
		EMMQueryRequestType_GetGroups,
		EMMQueryRequestType_GetServerByKey,
		EMMQueryRequestType_GetServerByIP,
		EMMQueryRequestType_SubmitData,
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
		mpMMQueryCB callback;
		void *extra;
	} MMQueryRequest;

	class MMQueryTask : public OS::Task<MMQueryRequest> {
		public:
			MMQueryTask();
			~MMQueryTask();
			static MMQueryTask *getQueryTask();

			static void FreeServerListQuery(struct MM::ServerListQuery *query);

			void AddDriver(SB::Driver *driver);
			void RemoveDriver(SB::Driver *driver);

		private:
			static void *TaskThread(OS::CThread *thread);

			static void *setup_redis_async(OS::CThread *thread);

			static void onRedisMessage(redisAsyncContext *c, void *reply, void *privdata);

			void AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, redisContext *redis_ctx, const sServerListReq *req);
			void AppendGroupEntry(const char *entry_name, ServerListQuery *ret, redisContext *redis_ctx, bool all_keys);

			bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key, redisContext *redis_ctx);
			bool FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx);
			bool FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx);

			Server *GetServerByKey(std::string key, redisContext *redis_ctx = NULL, bool include_deleted = false);
			Server *GetServerByIP(OS::Address address, OS::GameData game, redisContext *redis_ctx = NULL);

			ServerListQuery GetServers(const sServerListReq *req);
			ServerListQuery GetGroups(const sServerListReq *req);

			void PerformServersQuery(MMQueryRequest request);
			void PerformGroupsQuery(MMQueryRequest request);
			void PerformSubmitData(MMQueryRequest request);

			std::vector<SB::Driver *> m_drivers;
			redisContext *mp_redis_connection;
			redisContext *mp_redis_async_retrival_connection;
			redisAsyncContext *mp_redis_async_connection;

			static MMQueryTask *m_task_singleton;
	};
};

#endif //_MM_QUERY_H