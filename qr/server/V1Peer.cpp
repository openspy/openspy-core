#include <stdio.h>
#include <stdlib.h>

#include "QRServer.h"


#include "QRPeer.h"
#include "QRDriver.h"
#include "V1Peer.h"

#include <OS/legacy/helpers.h>

#include <sstream>

namespace QR {
	V1Peer::V1Peer(Driver *driver, INetIOSocket *socket) : Peer(driver, socket, 1) {
		memset(&m_challenge, 0, sizeof(m_challenge));
		gen_random((char *)&m_challenge, 6);
		m_sent_challenge = false;
		m_uses_validation = false;
		m_validated = false;
		m_query_state = EV1_CQS_Complete;

		m_server_info.m_game.secretkey[0] = 0;
		m_server_info.m_game.gameid = 0;
		m_server_info.m_address = socket->address;

		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_last_ping, NULL);
	}
	V1Peer::~V1Peer() {
	}

	void V1Peer::think(bool listener_waiting) {
		send_ping();
		SubmitDirtyServer();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);

		if (current_time.tv_sec - m_last_recv.tv_sec > QR1_PING_TIME * 2) {
			Delete(true);
		}
	}

	void V1Peer::handle_packet(INetIODatagram packet) {
		if (packet.error_flag) {
			Delete();
			return;
		}

		m_peer_stats.packets_in++;
		m_peer_stats.bytes_in += packet.buffer.remaining();


		std::string recv_buf = std::string((const char *)packet.buffer.GetHead(), packet.buffer.remaining());
		OS::KVReader data_parser = OS::KVReader(recv_buf);

		if (data_parser.Size() < 1) {
			Delete();
			return;
		}
		std::string command = data_parser.GetKeyByIdx(0);

		gettimeofday(&m_last_recv, NULL);

		if (command.compare("heartbeat") == 0) {
			handle_heartbeat(data_parser);
		}
		else if (command.compare("echo") == 0) {
			handle_echo(data_parser);
		}
		else if (command.compare("validate") == 0) {
			handle_validate(data_parser);
		}
		else {
			if (m_query_state != EV1_CQS_Complete) {
				handle_ready_query_state(data_parser);
			}
		}
	}

	void V1Peer::send_error(bool die, const char *fmt, ...) {
		std::ostringstream s;

		va_list args;
		va_start(args, fmt);
		char send_str[512]; //mtu size
		int len = vsnprintf(send_str, sizeof(send_str), fmt, args);
		send_str[len] = 0;
		va_end(args);

		s << "\\error\\" << send_str << "\\fatal\\" << die;
		SendPacket(s.str(), true);

		if (die)
			Delete();
	}
	void V1Peer::SendClientMessage(uint8_t *data, int data_len) {

	}
	void V1Peer::parse_rules(OS::KVReader data_parser) {
		m_dirty_server_info.m_keys = data_parser.GetKVMap();

		if(m_dirty_server_info.m_keys.find("echo") != m_dirty_server_info.m_keys.cend())
			m_dirty_server_info.m_keys.erase(m_dirty_server_info.m_keys.find("echo"));

		std::stringstream ss;
		std::map<std::string, std::string>::iterator it = m_dirty_server_info.m_keys.begin();
		while (it != m_dirty_server_info.m_keys.end()) {
			std::pair<std::string, std::string> p = *it;
			ss << "(" << p.first << ", " << p.second << ") ";
			it++;
		}
		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", m_sd->address.ToString().c_str(), ss.str().c_str());
	}
	void V1Peer::parse_players(OS::KVReader data_parser) {
		std::stringstream ss;

		std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it_pair = data_parser.GetHead();
		std::vector<std::pair< std::string, std::string> >::const_iterator it = it_pair.first;
		while (it != it_pair.second) {

			std::pair<std::string, std::string> p = *it;
			std::string key, value;
			key = p.first;
			value = p.second;

			std::string::size_type index_seperator = p.first.rfind('_');
			if (index_seperator != std::string::npos) {
				m_dirty_server_info.m_player_keys[p.first.substr(0, index_seperator + 1)].push_back(p.second);

				ss << "P(" << m_dirty_server_info.m_player_keys[p.first.substr(0, index_seperator + 1)].size()-1 << ") ( " << p.first << "," << p.second << ") ";
			}

			if (p.first.compare("final") == 0)
				break;
			it++;
		}


		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", m_sd->address.ToString().c_str(), ss.str().c_str());
	}
	void V1Peer::handle_ready_query_state(OS::KVReader data_parser) {
		std::ostringstream s;
		std::string challenge;

		if (!data_parser.HasKey("echo")) {
			send_error(true, "Missing echo param in query response");
			return;
		}
		else {
			challenge = OS::strip_whitespace(data_parser.GetValue("echo"), true);
			int ret = strcmp(challenge.c_str(), (char *)&m_challenge);
			if (ret) {
				send_error(true, "incorrect challenge response in query response");
				return;
			}
		}


		gettimeofday(&m_last_recv, NULL); //validated, update ping

		switch (m_query_state) {
		case EV1_CQS_Basic:
			m_dirty_server_info.m_keys.clear();
			m_dirty_server_info.m_player_keys.clear();
			s << "\\info\\";
			parse_rules(data_parser);
			m_query_state = EV1_CQS_Info;
			break;
		case EV1_CQS_Info:
			s << "\\rules\\";
			parse_rules(data_parser);
			m_query_state = EV1_CQS_Rules;
			break;
		case EV1_CQS_Rules:
			s << "\\players\\";
			parse_players(data_parser);
			m_query_state = EV1_CQS_Players;
			break;
		case EV1_CQS_Players:
			parse_players(data_parser);
			MM::MMPushRequest req;
			if (!m_server_pushed) {
				m_server_pushed = true;
				req.type = MM::EMMPushRequestType_PushServer;
			}
			else {
				req.type = MM::EMMPushRequestType_UpdateServer;
			}

			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if (current_time.tv_sec - m_last_heartbeat.tv_sec > HB_THROTTLE_TIME || req.type == MM::EMMPushRequestType_PushServer) {
				req.peer = this;
				req.server = m_dirty_server_info;
				req.old_server = m_server_info;
				m_server_info = m_dirty_server_info;

				req.peer->IncRef();

				m_server_info_dirty = false;
				gettimeofday(&m_last_heartbeat, NULL);
				m_peer_stats.pending_requests++;
				MM::m_task_pool->AddRequest(req);
			} else {
				m_server_info_dirty = true;
			}

			m_query_state = EV1_CQS_Complete;
			return;
			break;
		}

		gen_random((char *)&m_challenge, 6); //make new challenge
		s << "\\echo\\ " << m_challenge;
		SendPacket(s.str());
	}
	void V1Peer::handle_validate(OS::KVReader data_parser) {
		std::string validate = data_parser.GetValue("validate");

		unsigned char *validation = gsseckey(NULL, (unsigned char *)m_challenge, (unsigned char *)m_server_info.m_game.secretkey, 0);

		if (strcmp((const char *)validation, (const char *)validate.c_str()) == 0) {
			m_validated = true;
			m_query_state = EV1_CQS_Basic;
			std::stringstream s;
			s << "\\basic\\\\echo\\" << m_challenge;
			SendPacket(s.str(), true);

			gettimeofday(&m_last_recv, NULL); //validated, update ping
		}
		else {
			m_validated = false;
			send_error(true, "Validation failure");
		}
		free((void *)validation);
	}
	void V1Peer::handle_heartbeat(OS::KVReader data_parser) {
		std::string gamename;
		int query_port = data_parser.GetValueInt("heartbeat");
		int state_changed = data_parser.GetValueInt("statechanged");

		if (state_changed == 2) {
			Delete();
			return;
		}

		gamename = data_parser.GetValue("gamename");

		OS::LogText(OS::ELogLevel_Info, "[%s] HB: %s", m_sd->address.ToString().c_str(), data_parser.ToString().c_str());
		//m_server_info.m_game = OS::GetGameByName(gamename.c_str());

		if (m_server_info.m_game.secretkey[0] != 0) {
			this->OnGetGameInfo(m_server_info.m_game, (void *)state_changed);
		}
		else if(!m_sent_game_query){
			MM::MMPushRequest req;
			req.peer = this;
			req.server = m_server_info;
			req.extra = (void *)state_changed;
			req.gamename = gamename;
			m_sent_game_query = true;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_GetGameInfoByGameName;
			m_peer_stats.pending_requests++;
			MM::m_task_pool->AddRequest(req);
		}
	}
	void V1Peer::OnGetGameInfo(OS::GameData game_info, void *extra) {
		std::ostringstream s;
		int state_changed = (int)extra;
		m_peer_stats.from_game = game_info;
		m_server_info.m_game = game_info;

		m_dirty_server_info = m_server_info;
		if (m_server_info.m_game.secretkey[0] == 0) {
			send_error(true, "unknown game");
			return;
		}
		if (state_changed == 2) {
			Delete();
			return;
		}
		if (m_validated) {
			m_query_state = EV1_CQS_Basic;
			s << "\\basic\\\\echo\\" << m_challenge;
		}
		else {
			s << "\\secure\\" << m_challenge;
		}
		if (s.str().length() > 0)
			SendPacket(s.str());
	}
	void V1Peer::handle_echo(OS::KVReader data_parser) {
		std::string validation;
		std::ostringstream s;

		validation = OS::strip_whitespace(data_parser.GetValue("echo"), true);

		OS::LogText(OS::ELogLevel_Info, "[%s] Echo: %s", m_sd->address.ToString().c_str(), data_parser.ToString().c_str());

		if (memcmp(validation.c_str(), m_challenge, sizeof(m_challenge)) == 0) {
			gettimeofday(&m_last_recv, NULL);
			if (m_validated) {
				//already validated, ping request
				gettimeofday(&m_last_ping, NULL);
			}
			else { //just validated, recieve server info for MMPush
				m_validated = true;
				m_query_state = EV1_CQS_Basic;
				s << "\\basic\\";
				if (s.str().length() > 0)
					SendPacket(s.str(), false);
			}
		}
		else {
			if (!m_validated) {
				send_error(true, "Validation failure");
			}
		}
	}
	void V1Peer::send_ping() {
		struct timeval current_time;
		std::ostringstream s;

		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_ping.tv_sec > QR1_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			gen_random((char *)&m_ping_challenge, 6);
			s << "\\echo\\" << m_ping_challenge;
			SendPacket(s.str());
		}
	}
	void V1Peer::SendPacket(std::string str, bool attach_final) {
		std::string send_str = str;
		OS::Buffer buffer;
		if (attach_final) {
			send_str += "\\final\\";
		}

		m_peer_stats.packets_out++;
		m_peer_stats.bytes_out += send_str.length()+1;

		buffer.WriteBuffer((void *)send_str.c_str(), send_str.length());

		NetIOCommResp resp = GetDriver()->getServer()->getNetIOInterface()->datagramSend(m_sd, buffer);
		if (resp.disconnect_flag || resp.error_flag) {
			Delete();
		}
	}
	void V1Peer::OnRegisteredServer(int pk_id, void *extra) {
		m_server_info.id = pk_id;
		m_dirty_server_info = m_server_info;
	}
}