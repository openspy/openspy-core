#ifndef _MM_QUERY_H
#define _MM_QUERY_H
#include "main.h"
#include <vector>
#include <map>
#include <string>

namespace SB {
	struct sServerListReq;
	class Driver;
};


namespace MM {
	void AddDriver(SB::Driver *driver);
	void RemoveDriver(SB::Driver *driver);
	void Init();
	void Think(); //check for push notifications, etc
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

	struct ServerListQuery{
		std::vector<std::string> requested_fields;

		std::vector<std::string> captured_basic_fields;
		std::vector<std::string> captured_player_fields;
		std::vector<std::string> captured_team_fields;
		std::vector<Server *> list;
	};
	

	void AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted, redisContext *redis_ctx);
	bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key, redisContext *redis_ctx);
	bool FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx);
	bool FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index, redisContext *redis_ctx);

	void SubmitData(const char *base64, struct sockaddr_in *from, struct sockaddr_in *to, OS::GameData *game);
	
	struct MM::ServerListQuery GetServers(const SB::sServerListReq *req);
	struct MM::ServerListQuery GetGroups(const SB::sServerListReq *req);

	Server *GetServerByKey(std::string key, redisContext *redis_ctx = NULL, bool include_deleted = false);
	Server *GetServerByIP(OS::Address address, OS::GameData game, redisContext *redis_ctx = NULL);
	extern redisContext *mp_redis_connection;
};

#endif //_MM_QUERY_H