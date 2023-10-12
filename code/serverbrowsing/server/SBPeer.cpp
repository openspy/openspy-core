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
		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_version = version;
		
		uv_mutex_init(&mp_mutex);
		uv_async_init(uv_default_loop(), &mp_pending_request_flush_async, flush_pending_requests);
		uv_handle_set_data((uv_handle_t*)&mp_pending_request_flush_async, this);

		uv_mutex_init(&m_crypto_mutex);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
		uv_mutex_destroy(&mp_mutex);
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
		uv_mutex_lock(&mp_mutex);
		m_pending_request_list.push(req);
		uv_mutex_unlock(&mp_mutex);
		uv_async_send(&mp_pending_request_flush_async);
	}
	void Peer::FlushPendingRequests() {
		uv_mutex_lock(&mp_mutex);
		while (!m_pending_request_list.empty()) {
			// TaskScheduler<MM::MMQueryRequest, TaskThreadData> *scheduler = ((SBServer *)(GetDriver()->getServer()))->getScheduler();
			// MM::MMQueryRequest req = m_pending_request_list.front();
			// req.peer = this;
			// req.peer->IncRef();
			// req.driver = mp_driver;
			// scheduler->AddRequest(req.type, req);
			// m_pending_request_list.pop();
			uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));
			MM::MMQueryRequest req = m_pending_request_list.top();
			m_pending_request_list.pop();

			MM::MMWorkData *work_data = new MM::MMWorkData();
			req.peer = this;
			this->IncRef();
			work_data->request = req;

			uv_handle_set_data((uv_handle_t*) uv_req, work_data);

			uv_queue_work(uv_default_loop(), uv_req, MM::PerformUVWorkRequest, MM::PerformUVWorkRequestCleanup);
		}
		uv_mutex_unlock(&mp_mutex);
	}

	void Peer::flush_pending_requests(uv_async_t *handle) {
		
		Peer *peer = (Peer *)uv_handle_get_data((uv_handle_t*)handle);
		peer->FlushPendingRequests();
	}

	void Peer::Delete(bool timeout) {
		if(!m_delete_flag) {
			IncRef();
			uv_close((uv_handle_t*)&mp_pending_request_flush_async, close_callback);
		}
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}
