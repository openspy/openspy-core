#include "SBPeer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

namespace SB {
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd, int version) : INetPeer(driver, address_info, sd) {

		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_version = version;
		mp_mutex = OS::CreateMutex();

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection version %d",OS::Address(m_address_info).ToString().c_str(), m_version);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d",OS::Address(m_address_info).ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
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
		mp_mutex->lock();
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				mp_mutex->unlock();
				return cache;
			}
			it++;
		}
		mp_mutex->unlock();
		return ret;		
	}
	void Peer::DeleteServerFromCacheByIP(OS::Address address) {
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		mp_mutex->lock();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				it = m_visible_servers.erase(it);
				continue;
			}
			it++;
		}
		mp_mutex->unlock();
	}
	void Peer::DeleteServerFromCacheByKey(std::string key) {
		mp_mutex->lock();
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.key.compare(key) == 0) {
				it = m_visible_servers.erase(it);
				continue;
			}
			it++;
		}
		mp_mutex->unlock();
	}
	void Peer::cacheServer(MM::Server *server, bool full_keys) {
		sServerCache item;
		mp_mutex->lock();
		if(FindServerByIP(server->wan_address).key[0] == 0) {
			item.wan_address = server->wan_address;
			item.key = server->key;
			item.full_keys = full_keys;
			m_visible_servers.push_back(item);
		}
		mp_mutex->unlock();
	}
	sServerCache Peer::FindServerByKey(std::string key) {
		mp_mutex->lock();
		sServerCache ret;
		ret.full_keys = false;
		ret.key[0] = 0;
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.key.compare(key) == 0) {
				mp_mutex->unlock();
				return cache;
			}
			it++;
		}
		mp_mutex->unlock();
		return ret;		
	}
	void Peer::AddRequest(MM::MMQueryRequest req) {
		if (req.type != MM::EMMQueryRequestType_GetGameInfoByGameName && req.type != MM::EMMQueryRequestType_GetGameInfoPairByGameName) {
			if (m_game.gameid == 0) {
				m_pending_request_list.push(req);
				return;
			}
		}
		req.peer = this;
		req.peer->IncRef();
		req.driver = mp_driver;
		MM::m_task_pool->AddRequest(req);
	}
	void Peer::FlushPendingRequests() {
		while (!m_pending_request_list.empty()) {
			MM::MMQueryRequest req = m_pending_request_list.front();
			req.peer = this;
			req.peer->IncRef();
			req.driver = mp_driver;
			MM::m_task_pool->AddRequest(req);
			m_pending_request_list.pop();
		}
	}
}