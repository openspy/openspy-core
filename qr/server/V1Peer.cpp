#include <stdio.h>
#include <stdlib.h>

#include "QRServer.h"


#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/socketlib/socketlib.h>
#include "V1Peer.h"

#include <OS/legacy/buffwriter.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/helpers.h>

#include <sstream>
#include <OS/KVReader.h>

namespace QR {
	V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver, address_info, sd, 1) {
		memset(&m_challenge, 0, sizeof(m_challenge));
		gen_random((char *)&m_challenge, 6);
		m_sent_challenge = false;
		m_uses_validation = false;
		m_validated = false;
		m_query_state = EV1_CQS_Complete;
		m_pushed_server = false;


		m_server_info.m_game.gameid = 0;
		m_server_info.m_address = OS::Address(m_address_info);

		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_last_ping, NULL);
	}
	V1Peer::~V1Peer() {
	}

	void V1Peer::think(bool listener_waiting) {
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > QR1_PING_TIME * 2) {
			Delete();
			m_timeout_flag = true;
		}
	}

	void V1Peer::handle_packet(char *recvbuf, int len) {
		if (len < 0) {
			Delete();
			return;
		}
		else if (len == 0) {
			return;
		}

		OS::KVReader data_parser = OS::KVReader(std::string(recvbuf));

		std::string command = data_parser.GetValueByIdx(0);


		//gettimeofday(&m_last_recv, NULL); //not here due to spoofing

		if (command.compare("heartbeat") == 0) {
			handle_heartbeat(recvbuf, len);
		}
		else if (command.compare("echo") == 0) {
			handle_echo(recvbuf, len);
		}
		else {
			if (m_query_state != EV1_CQS_Complete) {
				handle_ready_query_state(recvbuf, len);
			}
		}
	}

	void V1Peer::send_error(bool die, const char *fmt, ...) {
		std::ostringstream s;
		if (die)
			Delete();


		s << "\\error\\" << fmt << "\\fatal\\" << m_delete_flag;
		SendPacket((const uint8_t*)s.str().c_str(), s.str().length(), true);
	}
	void V1Peer::SendClientMessage(uint8_t *data, int data_len) {

	}
	void V1Peer::parse_rules(char *recvbuf, int len) {
		m_server_info.m_keys = OS::KeyStringToMap(recvbuf);
	}
	void V1Peer::parse_players(char *recvbuf, int len) {

		OS::KVReader data_parser = OS::KVReader(std::string(recvbuf));

		std::pair<std::unordered_map<std::string, std::string>::const_iterator, std::unordered_map<std::string, std::string>::const_iterator> it_pair = data_parser.GetHead();
		std::unordered_map<std::string, std::string>::const_iterator it = it_pair.first;
		while (it != it_pair.second) {

			std::pair<std::string, std::string> p = *it;
			std::string key, value;
			key = p.first;
			value = p.second;

			std::string::size_type index_seperator = p.first.rfind('_');

			if (index_seperator != std::string::npos) {
				m_server_info.m_player_keys[p.first.substr(0, index_seperator + 1)].push_back(p.second);
			}

			if (p.first.compare("final") == 0)
				break;
			it++;
		}
	}
	void V1Peer::handle_ready_query_state(char *recvbuf, int len) {
		std::ostringstream s;
		std::string challenge;

		OS::KVReader data_parser = OS::KVReader(std::string(recvbuf));

		if (!data_parser.HasKey("echo")) {
			send_error(true, "Missing echo param in query response");
			return;
		}
		else {
			challenge = data_parser.GetValue("echo");
			if (strcmp(OS::strip_whitespace(challenge).c_str(), (char *)&m_challenge)) {
				send_error(true, "incorrect challenge response in query response");
				return;
			}
		}


		gettimeofday(&m_last_recv, NULL);
		switch (m_query_state) {
		case EV1_CQS_Basic:
			m_server_info.m_keys.clear();
			m_server_info.m_player_keys.clear();
			s << "\\info\\";
			parse_rules(recvbuf, len);
			m_query_state = EV1_CQS_Info;
			break;
		case EV1_CQS_Info:
			s << "\\rules\\";
			parse_rules(recvbuf, len);
			m_query_state = EV1_CQS_Rules;
			break;
		case EV1_CQS_Rules:
			s << "\\players\\";
			parse_players(recvbuf, len);
			m_query_state = EV1_CQS_Players;
			break;
		case EV1_CQS_Players:
			parse_players(recvbuf, len);
			MM::MMPushRequest req;
			req.peer = this;
			req.server = &m_server_info;

			req.peer->IncRef();
			if (!m_pushed_server) {
				m_pushed_server = true;
				req.type = MM::EMMPushRequestType_PushServer;
			}
			else {
				req.type = MM::EMMPushRequestType_UpdateServer;
			}

			MM::m_task_pool->AddRequest(req);
			m_query_state = EV1_CQS_Complete;
			return;
			break;
		}

		gen_random((char *)&m_challenge, 6); //make new challenge
		s << "\\echo\\ " << m_challenge;
		SendPacket((const uint8_t*)s.str().c_str(), s.str().length(), false);
	}
	void V1Peer::handle_heartbeat(char *recvbuf, int len) {
		
		std::string gamename;
		OS::KVReader data_parser = OS::KVReader(std::string(recvbuf));
		int query_port = data_parser.GetValueInt("heartbeat");
		int state_changed = data_parser.GetValueInt("statechanged");

		gamename = data_parser.GetValue("gamename");

		OS::LogText(OS::ELogLevel_Info, "[%s] HB: %s", OS::Address(m_address_info).ToString().c_str(), recvbuf);
		//m_server_info.m_game = OS::GetGameByName(gamename.c_str());

		if (m_server_info.m_game.gameid != 0) {
			this->OnGetGameInfo(m_server_info.m_game, (void *)state_changed);
		}
		else if(!m_sent_game_query){
			MM::MMPushRequest req;
			req.peer = this;
			req.server = &m_server_info;
			req.extra = (void *)state_changed;
			req.gamename = gamename;
			m_sent_game_query = true;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_GetGameInfoByGameName;
			MM::m_task_pool->AddRequest(req);
		}
	}
	void V1Peer::OnGetGameInfo(OS::GameData game_info, void *extra) {
		std::ostringstream s;
		int state_changed = (int)extra;
		m_server_info.m_game = game_info;
		if (!m_server_info.m_game.gameid) {
			send_error(true, "unknown game");
			return;
		}

		if (state_changed == 1) {
			m_query_state = EV1_CQS_Basic;
			if (m_validated) {
				s << "\\basic\\\\echo\\" << m_challenge;
			}
			else {
				if (!m_uses_validation) {
					s << "\\basic\\\\echo\\" << m_challenge;
				}
				else {
					s << "\\basic\\\\secure\\" << m_challenge;
				}

			}
		}
		else if (state_changed == 2) {
			m_validated = false;
			Delete();
			return;
		}

		if (s.str().length() > 0)
			SendPacket((const uint8_t*)s.str().c_str(), s.str().length(), false);
	}
	void V1Peer::handle_echo(char *recvbuf, int len) {
		std::string validation;
		std::ostringstream s;

		OS::KVReader data_parser = OS::KVReader(std::string(recvbuf));
		validation = data_parser.GetValue("echo");

		OS::LogText(OS::ELogLevel_Info, "[%s] Echo: %s", OS::Address(m_address_info).ToString().c_str(), recvbuf);

		if (memcmp(validation.c_str(), m_challenge, sizeof(m_challenge)) == 0) {
			gettimeofday(&m_last_recv, NULL);
			if (m_validated) {
				//already validated, ping request
				gettimeofday(&m_last_ping, NULL);
			}
			else { //just validated, recieve server info for MMPush
				m_validated = true;
				m_query_state = EV1_CQS_Basic;
				s << "\\basic\\\\echo\\" << m_challenge;
				if (s.str().length() > 0)
					SendPacket((const uint8_t*)s.str().c_str(), s.str().length(), false);
			}
		}
		else {
			if (!m_validated) {
				send_error(true, "Validation failure");
			}
		}
	}
	void V1Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		std::ostringstream s;

		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_ping.tv_sec > QR1_PING_TIME) {
			gen_random((char *)&m_ping_challenge, 6);
			s << "\\echo\\" << m_ping_challenge;
			SendPacket((const uint8_t*)s.str().c_str(), s.str().length(), false);
		}
	}
	void V1Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		BufferWriteData(&p, &out_len, buff, len);
		if (attach_final) {
			BufferWriteData(&p, &out_len, (uint8_t*)"\\final\\", 7);
		}
		int c = sendto(m_sd, (char *)&out_buff, out_len, 0, (struct sockaddr *)&m_address_info, sizeof(sockaddr_in));
		if (c < 0) {
			Delete();
		}
	}
	void V1Peer::Delete() {
		if (m_server_pushed) {
			MM::MMPushRequest req;
			req.peer = this;
			req.server = &m_server_info;
			req.peer->IncRef();
			req.type = MM::EMMPushRequestType_DeleteServer;
			MM::m_task_pool->AddRequest(req);
		}
		m_delete_flag = true;
	}
}