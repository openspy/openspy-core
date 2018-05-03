#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/helpers.h>


namespace QR {
	Peer::Peer(Driver *driver, INetIOSocket *sd, int version) : INetPeer(driver, sd) {
		m_server_pushed = false;
		m_delete_flag = false;
		m_timeout_flag = false;
		m_server_info.id = -1;
		m_server_info.groupid = 0;
		m_server_info.m_game.gameid = 0;
		m_server_info.m_game.gamename[0] = 0;
		m_sent_game_query = false;
		m_version = version;

		m_peer_stats.m_address = sd->address;
		m_peer_stats.version = version;
		m_peer_stats.bytes_in = 0;
		m_peer_stats.bytes_out = 0;
		m_peer_stats.packets_in = 0;
		m_peer_stats.packets_out = 0;
		m_peer_stats.pending_requests = 0;
		m_peer_stats.from_game.gamename[0] = 0;
		m_peer_stats.from_game.gameid = 0;
		m_peer_stats.disconnected = false;

		m_server_info_dirty = false;

		memset(&m_last_heartbeat,0,sizeof(m_last_heartbeat));

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection version: %d",m_sd->address.ToString().c_str(), m_version);
	}
	Peer::~Peer() {
		Delete();
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
	}
	bool Peer::isTeamString(const char *string) {
		int len = strlen(string);
		if(len < 2)
			return false;
		if(string[len-2] == '_' && string[len-1] == 't') {
			return true;
		}
		return false;
	}
	OS::MetricValue Peer::GetMetricItemFromStats(PeerStats stats) {
		OS::MetricValue arr_value, value;
		value.value._str = stats.m_address.ToString(false);
		value.key = "ip";
		value.type = OS::MetricType_String;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.disconnected;
		value.key = "disconnected";
		value.type = OS::MetricType_Integer;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));		

		value.type = OS::MetricType_Integer;
		value.value._int = stats.bytes_in;
		value.key = "bytes_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.bytes_out;
		value.key = "bytes_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_in;
		value.key = "packets_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_out;
		value.key = "packets_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.pending_requests;
		value.key = "pending_requests";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));
		arr_value.type = OS::MetricType_Array;
			
		value.type = OS::MetricType_String;	
		value.key = "gamename";
		value.value._str = stats.from_game.gamename;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;	
		value.key = "version";
		value.value._int = stats.version;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		arr_value.key = stats.m_address.ToString(false);
		return arr_value;
	}
	void Peer::ResetMetrics() {
		m_peer_stats.bytes_in = 0;
		m_peer_stats.bytes_out = 0;
		m_peer_stats.packets_in = 0;
		m_peer_stats.packets_out = 0;
		m_peer_stats.pending_requests = 0;
	}
	OS::MetricInstance Peer::GetMetrics() {
		OS::MetricInstance peer_metric;

		peer_metric.value = GetMetricItemFromStats(m_peer_stats);
		peer_metric.key = "peer";

		ResetMetrics();

		return peer_metric;
	}
	void Peer::SubmitDirtyServer() {
		if(!m_server_info_dirty)
			return;
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_heartbeat.tv_sec > HB_THROTTLE_TIME) {
			MM::MMPushRequest req;
			req.old_server = m_server_info;
			m_server_info = m_dirty_server_info;
			m_server_info_dirty = false;
			req.peer = this;

			req.server = m_server_info;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_UpdateServer;
			m_peer_stats.pending_requests++;
			MM::m_task_pool->AddRequest(req);
		}
	}
	void Peer::Delete(bool timeout, bool disconnect) {
		if (m_server_pushed) {
			MM::MMPushRequest req;
			req.peer = this;
			req.server = m_server_info;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_DeleteServer;
			m_peer_stats.pending_requests++;
			MM::m_task_pool->AddRequest(req);
		}
		m_server_pushed = false;
		m_timeout_flag = timeout;
		m_delete_flag = disconnect;
	}
}