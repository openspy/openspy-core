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
		
		mp_mutex = OS::CreateMutex();
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
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
	void Peer::FlushPendingRequests() {
		while (!m_pending_request_list.empty()) {
			TaskScheduler<MM::MMQueryRequest, TaskThreadData> *scheduler = ((SBServer *)(GetDriver()->getServer()))->getScheduler();
			MM::MMQueryRequest req = m_pending_request_list.front();
			req.peer = this;
			req.peer->IncRef();
			req.driver = mp_driver;
			scheduler->AddRequest(req.type, req);
			m_pending_request_list.pop();
		}
	}

	void Peer::Delete(bool timeout) {
		CloseSocket(); //closed for improved V1 speeds
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
}
