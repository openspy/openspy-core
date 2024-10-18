#include "SBServer.h"
#include "SBPeer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>

#include <tasks/tasks.h>

#include <serverbrowsing/filter/filter.h>

namespace SB {
	Peer::Peer(Driver *driver, uv_tcp_t *sd, int version) : INetPeer(driver, sd) {
		mp_driver = driver;
		m_delete_flag = false;
		m_timeout_flag = false;
		uv_clock_gettime(UV_CLOCK_MONOTONIC, &m_last_ping);
		uv_clock_gettime(UV_CLOCK_MONOTONIC, &m_last_recv);

		m_version = version;

		uv_mutex_init(&m_crypto_mutex);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
		uv_mutex_destroy(&m_crypto_mutex);
	}

	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection version %d",getAddress().ToString().c_str(), m_version);
	}

	bool Peer::serverMatchesLastReq(MM::Server *server, bool require_push_flag) {
		/*
		if(require_push_flag && !m_last_list_req.push_updates) {
			return false;
		}*/
		if(server->game.gameid == m_last_list_req.m_for_game.gameid) {
			if(m_last_list_req.filter.length() == 0 || filterMatches(m_last_list_req_token_list, server->kvFields)) {
				return true;
			}
			
		}
		return false;
	}
	void Peer::AddRequest(MM::MMQueryRequest req) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		MM::MMWorkData *work_data = new MM::MMWorkData();
		req.peer = this;
		this->IncRef();

		work_data->request = req;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);

		uv_queue_work(uv_default_loop(), uv_req, MM::PerformUVWorkRequest, MM::PerformUVWorkRequestCleanup);
	}

	void Peer::Delete(bool timeout) {
		if(!m_delete_flag) {
			IncRef();
		}
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}
