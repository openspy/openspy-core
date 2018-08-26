#include "SBPeer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>

namespace SB {
	Peer::Peer(Driver *driver, INetIOSocket *sd, int version) : INetPeer(driver, sd) {
		mp_driver = driver;
		m_delete_flag = false;
		m_timeout_flag = false;
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_version = version;
		
		mp_mutex = OS::CreateMutex();

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection version %d",m_sd->address.ToString().c_str(), m_version);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
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
	void Peer::AddRequest(MM::MMQueryRequest req) {
		if (req.type != MM::EMMQueryRequestType_GetGameInfoByGameName && req.type != MM::EMMQueryRequestType_GetGameInfoPairByGameName) {
			if (m_game.secretkey[0] == 0) {
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

	void Peer::Delete(bool timeout) {
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}