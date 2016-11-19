#include "SBPeer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <sstream>
#define CRYPTCHAL_LEN 10
#define SERVCHAL_LEN 25
namespace SB {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		m_delete_flag = false;
		m_timeout_flag = false;
		m_address_info = *address_info;
		gettimeofday(&m_last_ping, NULL);
		printf("new Peer\n");
	}
	Peer::~Peer() {
		close(m_sd);
		printf("Peer delete\n");
	}

	bool Peer::serverMatchesLastReq(MM::Server *server, bool require_push_flag) {
		/*
		if(require_push_flag && !m_last_list_req.push_updates) {
			return false;
		}*/
		if(server->game.gameid == m_last_list_req.m_for_game.gameid) {
			return true;
		}
		return false;
	}
	sServerCache Peer::FindServerByIP(OS::Address address) {
		sServerCache ret;
		ret.full_keys = false;
		ret.key[0] = 0;
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				return cache;
			}
			it++;
		}
		return ret;		
	}
	void Peer::DeleteServerFromCacheByIP(OS::Address address) {
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				it = m_visible_servers.erase(it);
				continue;
			}
			it++;
		}
	}
	void Peer::cacheServer(MM::Server *server) {
		sServerCache item;
		if(FindServerByIP(server->wan_address).key[0] == 0) {
			item.wan_address = server->wan_address;
			strcpy(item.key,server->key.c_str());
			item.full_keys = false;
			m_visible_servers.push_back(item);
		}
	}
}