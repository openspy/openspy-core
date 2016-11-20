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

namespace QR {
	V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver,address_info, sd) {
		memset(&m_challenge,0,sizeof(m_challenge));
		gen_random((char *)&m_challenge,6);
		m_sent_challenge = false;
		m_uses_validation = false;
		m_validated = false;
		m_query_state = EV1_CQS_Complete;
		m_pushed_server = false;

		m_server_info.m_address.port = Socket::htons(m_address_info.sin_port);
		m_server_info.m_address.ip = Socket::htonl(m_address_info.sin_addr.s_addr);

		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_last_ping, NULL);
	}
	V1Peer::~V1Peer() {
		if(m_server_pushed) {
			MM::DeleteServer(&m_server_info);
		}
	}
	
	void V1Peer::think() {
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > QR1_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
		}
	}

	void V1Peer::handle_packet(char *recvbuf, int len) {
		if(len < 0) {
			m_delete_flag = true;
			return;
		} else if(len == 0) {
			return;
		}

		char command[32];
		//gettimeofday(&m_last_recv, NULL); //not here due to spoofing
		if(find_param(0, recvbuf,(char *)&command, sizeof(command))) {
		}
		if(strcmp(command,"heartbeat") == 0) {
			handle_heartbeat(recvbuf, len);
		} else if(strcmp(command, "echo") == 0) {
			handle_echo(recvbuf, len);
		} else {
			if(m_query_state != EV1_CQS_Complete) {
				handle_ready_query_state(recvbuf, len);
			}
		}
	}

	void V1Peer::send_error(bool die, const char *fmt, ...) {
		std::ostringstream s;
		if(die) 
			m_delete_flag = true;


		s << "\\error\\" << fmt << "\\fatal\\" << m_delete_flag;
		SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), true);
	}
	void V1Peer::SendClientMessage(uint8_t *data, int data_len) {

	}
	void V1Peer::parse_rules(char *recvbuf, int len) {
		int i = 0;
		char data[KV_MAX_LEN + 1];

		std::string key, value;

		while(find_param(i++,recvbuf, (char *)&data, sizeof(data))) {
			if((i % 2) != 0) {
				key = std::string(data);
			} else {
				value = std::string(data);
				m_server_info.m_keys[key] = value;
			}

			if(key.compare("final") == 0)
				break;
		}
	}
	void V1Peer::parse_players(char *recvbuf, int len) {
		int i = 0;
		char data[KV_MAX_LEN + 1];

		std::string key, value;

		m_server_info.m_player_keys.clear();

		while(find_param(i++,recvbuf, (char *)&data, sizeof(data))) {
			if((i % 2) != 0) {
				key = std::string(data);
			} else {
				value = std::string(data);
				std::string::size_type index_seperator = key.rfind('_');
				int index = 0;
				if (index_seperator != std::string::npos) {
					index = atoi(key.substr(index_seperator+1).c_str());
					m_server_info.m_player_keys[key.substr(0,index_seperator+1)].push_back(std::string(value));
				}
			}

			if(key.compare("final") == 0)
				break;
		}
	}
	void V1Peer::handle_ready_query_state(char *recvbuf, int len) {
		std::ostringstream s;
		char challenge[32];
		if(!find_param("echo",(char *)recvbuf, (char *)&challenge,sizeof(challenge))) {
			send_error(true, "Missing echo param in query response");
			return;
		} else {
			if(strcmp(OS::strip_whitespace(challenge).c_str(), (char *)&m_challenge)) {
				send_error(true, "incorrect challenge response in query response");
				return;
			}
		}
		gettimeofday(&m_last_recv, NULL);
		switch(m_query_state) {
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
				if(!m_pushed_server) {
					m_pushed_server = true;
					printf("Push server\n");
					MM::PushServer(&m_server_info, true);	
				} else {
					printf("update server\n");
					MM::UpdateServer(&m_server_info);
				}
				m_query_state = EV1_CQS_Complete;
				return;
				break;
		}
		s << "\\echo\\ " << m_challenge;
		SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), false);
	}
	void V1Peer::handle_heartbeat(char *recvbuf, int len) {
		std::ostringstream s;
		char gamename[32];
		int query_port = find_paramint("heartbeat",(char *)recvbuf);
		int state_changed = find_paramint("statechanged",(char *)recvbuf);

		find_param("gamename",(char *)recvbuf, (char *)&gamename,sizeof(gamename));
		m_server_info.m_game = OS::GetGameByName(gamename);
		if(!m_server_info.m_game.gameid) {
			send_error(true, "unknown game");
			return;
		}

		int slen = sizeof(m_address_info);
		if(state_changed == 1) {
			m_query_state = EV1_CQS_Basic;
			if(m_validated) {
				s << "\\basic\\\\echo\\" << m_challenge;	
			} else {
				if(!m_uses_validation) {
					s << "\\basic\\\\echo\\" << m_challenge;	
				} else {
					s << "\\basic\\\\secure\\" << m_challenge;
  				}
				
			}
		} else if(state_changed == 2) {
			m_validated = false;
			MM::DeleteServer(&m_server_info, true);
			return;
		}

		if(s.str().length() > 0)
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), false);
		
	}
	void V1Peer::handle_echo(char *recvbuf, int len) {
		char validation[sizeof(m_challenge)];
		std::ostringstream s;
		find_param("echo",(char *)recvbuf, (char *)&validation,sizeof(validation));

		if(memcmp(validation, m_challenge, sizeof(m_challenge)) == 0) {
			gettimeofday(&m_last_recv, NULL);
			if(m_validated) {
				//already validated, ping request
				gettimeofday(&m_last_ping, NULL);
			} else { //just validated, recieve server info for MMPush
				m_validated = true;
				m_query_state = EV1_CQS_Basic;
				s << "\\basic\\\\echo\\" << m_challenge;	
				if(s.str().length() > 0)
					SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), false);
			}
		} else {
			if(!m_validated) {
				send_error(true, "Validation failure");
			}
		}
	}
	void V1Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		std::ostringstream s;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > QR1_PING_TIME) {
			gen_random((char *)&m_ping_challenge,6);
			s << "\\echo\\" << m_ping_challenge;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), false);
		}
	}
	void V1Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		int header_len = 0;
		BufferWriteData(&p, &out_len, buff, len);
		if(attach_final) {
			BufferWriteData(&p, &out_len, (uint8_t*)"\\final\\", 7);
		}
		int c = sendto(m_sd,(char *)&out_buff,out_len,0,(struct sockaddr *)&m_address_info, sizeof(sockaddr_in));
		if(c < 0) {
			m_delete_flag = true;
		}
	}
}