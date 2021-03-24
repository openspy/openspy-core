#include "QRPeer.h"
#include "QRDriver.h"
#include "QRServer.h"
#include <tasks/tasks.h>

namespace QR {
	Peer::Peer(Driver *driver, INetIOSocket *sd, int version) : INetPeer(driver, sd) {
		m_server_pushed = false;
		m_delete_flag = false;
		m_timeout_flag = false;
		m_server_info.id = -1;
		m_server_info.groupid = 0;
		m_server_info.m_game.gameid = 0;
		m_server_info.m_game.gamename[0] = 0;
		m_server_info.m_game.secretkey[0] = 0;
		m_sent_game_query = false;
		m_version = version;

		m_server_info_dirty = false;
		mp_mutex = OS::CreateMutex();

		memset(&m_last_heartbeat,0,sizeof(m_last_heartbeat));
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::OnConnectionReady() {
		OS::LogText(OS::ELogLevel_Info, "[%s] New connection version: %d",getAddress().ToString().c_str(), m_version);
	}
	bool Peer::isTeamString(const char *string) {
		size_t len = strlen(string);
		if(len < 2)
			return false;
		if(string[len-2] == '_' && string[len-1] == 't') {
			return true;
		}
		return false;
	}
	void Peer::SubmitDirtyServer() {
		if(!m_server_info_dirty)
			return;
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_heartbeat.tv_sec > HB_THROTTLE_TIME) {
			TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(GetDriver()->getServer()))->getScheduler();
			MM::MMPushRequest req;
			req.old_server = m_server_info;
			m_server_info = m_dirty_server_info;
			m_server_info_dirty = false;
			req.peer = this;

			req.server = m_server_info;
			req.peer->IncRef();
			if (!m_server_pushed) {
				m_server_pushed = true;
				req.type = MM::EMMPushRequestType_PushServer;
			}
			else {
				req.type = MM::EMMPushRequestType_UpdateServer;
			}

			scheduler->AddRequest(req.type, req);
		}
	}
	void Peer::Delete(bool timeout) {
		DeleteServer();
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
	void Peer::DeleteServer() {
		if (m_server_pushed) {
			TaskScheduler<MM::MMPushRequest, TaskThreadData> *scheduler = ((QR::Server *)(GetDriver()->getServer()))->getScheduler();
			MM::MMPushRequest req;
			m_server_pushed = false;
			req.peer = this;
			req.server = m_server_info;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_DeleteServer;
			scheduler->AddRequest(MM::EMMPushRequestType_DeleteServer, req);
		}
		m_sent_game_query = false;
		
	}
}