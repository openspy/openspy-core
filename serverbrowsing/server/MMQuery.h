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
	void Init(SB::Driver *driver);
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
		std::vector<Server *> list;
	};
	

	void AppendServerEntry(std::string entry_name, ServerListQuery *ret, bool all_keys, bool include_deleted = false);
	bool FindAppend_ServKVFields(Server *server, std::string entry_name, std::string key);
	bool FindAppend_PlayerKVFields(Server *server, std::string entry_name, std::string key, int index);
	bool FindAppend_TeamKVFields(Server *server, std::string entry_name, std::string key, int index);

	void SubmitData(const char *base64, struct sockaddr_in *from, struct sockaddr_in *to, OS::GameData *game);
	
	struct MM::ServerListQuery GetServers(const SB::sServerListReq *req);
	struct MM::ServerListQuery GetGroups(const SB::sServerListReq *req);

	Server *GetServerByKey(std::string key);
	extern SB::Driver *mp_driver;
	extern redisContext *mp_redis_connection;
};

#endif //_MM_QUERY_H