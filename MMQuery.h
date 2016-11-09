#ifndef _MM_QUERY_H
#define _MM_QUERY_H
#include "main.h"
#include <vector>
#include <map>
#include <string>

namespace SB {
	struct sServerListReq;
};


namespace MM {
	typedef struct {
		uint32_t ip;
		uint16_t port;
	} Address;
	void Init();
	typedef struct {
		Address wan_address;
		Address lan_address;
		OS::GameData *game;
		OS::countryRegion region;
		std::map<std::string, std::string> kvFields;
		
	} Server;

	struct ServerListQuery{
		std::vector<std::string> requested_fields;
		std::vector<Server *> list;
	};
	
	struct MM::ServerListQuery GetServers(const SB::sServerListReq *req);
	struct MM::ServerListQuery GetGroups(const SB::sServerListReq *req);
};

#endif //_MM_QUERY_H