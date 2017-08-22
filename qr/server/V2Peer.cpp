#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "QRServer.h"


#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/socketlib/socketlib.h>
#include "V2Peer.h"

#include <OS/legacy/buffwriter.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/helpers.h>

#define PACKET_QUERY              0x00  //S -> C
#define PACKET_CHALLENGE          0x01  //S -> C
#define PACKET_ECHO               0x02  //S -> C (purpose..?)
#define PACKET_ECHO_RESPONSE      0x05  // 0x05, not 0x03 (order) | C -> S
#define PACKET_HEARTBEAT          0x03  //C -> S
#define PACKET_ADDERROR           0x04  //S -> C
#define PACKET_CLIENT_MESSAGE     0x06  //S -> C
#define PACKET_CLIENT_MESSAGE_ACK 0x07  //C -> S
#define PACKET_KEEPALIVE          0x08  //S -> C | C -> S
#define PACKET_PREQUERY_IP_VERIFY 0x09  //S -> C
#define PACKET_AVAILABLE          0x09  //C -> S
#define PACKET_CLIENT_REGISTERED  0x0A  //S -> C

#define QR2_OPTION_USE_QUERY_CHALLENGE 128 //backend flag

#define QR_MAGIC_1 0xFE
#define QR_MAGIC_2 0xFD

namespace QR {
	V2Peer::V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver,address_info,sd,2) {
		m_recv_instance_key = false;

		m_sent_challenge = false;
		m_server_pushed = false;
		memset(&m_instance_key, 0, sizeof(m_instance_key));
		memset(&m_challenge, 0, sizeof(m_challenge));

		m_server_info.m_address.port = Socket::htons(m_address_info.sin_port);
		m_server_info.m_address.ip = Socket::htonl(m_address_info.sin_addr.s_addr);

		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

	}
	V2Peer::~V2Peer() {
	}

	void V2Peer::SendPacket(const uint8_t *buff, int len) {
		char sendbuf[MAX_DATA_SIZE + 1];
		uint8_t *p = (uint8_t *)&sendbuf;
		int blen = 0;
		BufferWriteByte((uint8_t**)&p,&blen,QR_MAGIC_1);
		BufferWriteByte((uint8_t**)&p,&blen,QR_MAGIC_2);
		BufferWriteData((uint8_t**)&p,&blen,buff, len);
		sendto(m_sd,(char *)&sendbuf,blen,0,(struct sockaddr *)&m_address_info, sizeof(sockaddr_in));
	}
	void V2Peer::handle_packet(char *recvbuf, int len) {
		uint8_t *buff = (uint8_t *)recvbuf;
		int buflen = len;

		uint8_t type = BufferReadByte((uint8_t**)&buff,&buflen);

		if(m_recv_instance_key) {
			uint8_t instance_key[REQUEST_KEY_LEN];
			BufferReadData((uint8_t **)&buff, (int *)&len, (uint8_t *)&instance_key, sizeof(instance_key));
			if(memcmp((uint8_t *)&instance_key, (uint8_t *)&m_instance_key, sizeof(instance_key)) != 0) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Instance key mismatch/possible spoofed packet", OS::Address(m_address_info).ToString().c_str());
				return;
			}
		}

		gettimeofday(&m_last_recv, NULL);

		switch(type) {
			case PACKET_AVAILABLE:
				handle_available((char *)buff, buflen);
				break;
			case PACKET_HEARTBEAT:
				handle_heartbeat((char *)buff, buflen);
			break;
			case PACKET_CHALLENGE:
				handle_challenge((char *)buff, buflen);
			break;
			case PACKET_KEEPALIVE:
				handle_keepalive((char *)buff, buflen);
			break;
			case PACKET_CLIENT_MESSAGE_ACK:
				OS::LogText(OS::ELogLevel_Info, "[%s] Client Message ACK", OS::Address(m_address_info).ToString().c_str());
			break;
		}
	}
	void V2Peer::handle_challenge(char *buff, int len) {
		char challenge_resp[90] = { 0 };
		int outlen = 0;
		uint8_t *p = (uint8_t *)challenge_resp;
		if(m_server_info.m_game.gameid == 0) {
			send_error(true, "Unknown game");
			return;
		}
		gsseckey((unsigned char *)&challenge_resp, (unsigned char *)&m_challenge, (unsigned char *)&m_server_info.m_game.secretkey, 0);
		if(strcmp(buff,challenge_resp) == 0) { //matching challenge
			OS::LogText(OS::ELogLevel_Info, "[%s] Server pushed, gamename: %s", OS::Address(m_address_info).ToString().c_str(), m_server_info.m_game.gamename);
			BufferWriteByte((uint8_t**)&p, &outlen,PACKET_CLIENT_REGISTERED);
			BufferWriteData((uint8_t **)&p, &outlen, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			SendPacket((uint8_t *)&challenge_resp, outlen);
			if(m_sent_challenge) {
				MM::MMPushRequest req;
				req.peer = this;
				req.server = &m_server_info;
				req.peer->IncRef();
				req.type = MM::EMMPushRequestType_PushServer;
				m_server_pushed = true;
				MM::m_task_pool->AddRequest(req);
			}
			m_sent_challenge = true;
		}
		else {
			OS::LogText(OS::ELogLevel_Info, "[%s] Incorrect challenge for gamename: %s", OS::Address(m_address_info).ToString().c_str(), m_server_info.m_game.gamename);
		}
	}
	void V2Peer::handle_keepalive(char *buff, int len) {
		uint32_t *key = (uint32_t *)buff;
		if(*key == *(uint32_t *)&m_instance_key)
			SendPacket((uint8_t*)buff, len);
	}
	void V2Peer::handle_heartbeat(char *buff, int len) {
		unsigned int i = 0;
		uint8_t *x;

		std::map<std::string, std::string> server_keys;
		std::map<std::string, std::vector<std::string> > player_keys;
		std::map<std::string, std::vector<std::string> > team_keys;

		std::string key, value;

		if(!m_recv_instance_key) {
			BufferReadData((uint8_t **)&buff, (int *)&len, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			m_recv_instance_key = true;
		}

		std::stringstream ss;

		while((buff[0] != 0 && len > 0) || (i%2 != 0)) {

			x = BufferReadNTS((uint8_t **)&buff,&len);

			if(i%2 == 0) {
				key = std::string((char *)x);
			} else {
				value = std::string((char *)x);
				ss << "(" << key << "," << value << ") ";
			}

			if(value.length() > 0) {
				server_keys[key] = value;
				value = std::string();
			}
			free((void *)x);
			i++;
		}

		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", OS::Address(m_address_info).ToString().c_str(), ss.str().c_str());
		ss.str("");


		uint16_t num_values = 0;
		BufferReadByte((uint8_t**)&buff,&len); //skip null byte(seperator)
		while((num_values = BufferReadShortRE((uint8_t**)&buff,&len))) {
			std::vector<std::string> nameValueList;
			if(len <= 3) {
				break;
			}
			uint32_t num_keys = 0;
			while(buff[0] != 0 && len > 0) {
				x = BufferReadNTS((uint8_t **)&buff,&len);
				nameValueList.push_back(std::string((const char *)x));
				free((void *)x);
				num_keys++;
				if(buff[0] == 0) break;
			}
			unsigned int player=0,num_keys_t = num_keys,num_values_t = num_values*num_keys;
			i = 0;
			BufferReadByte((uint8_t**)&buff,&len);
			while(num_values_t--) {
				std::string name = nameValueList.at(i);

				x = BufferReadNTS((uint8_t **)&buff,&len);

				if(isTeamString(name.c_str())) {
					if(team_keys[name].size() <= player) {
						team_keys[name].push_back(std::string((const char *)x));
					}
					else {
						team_keys[name][player] = std::string((const char *)x);
					}
					ss << "T(" << player << ") (" << name.c_str() << "," << x << ") ";
				} else {
					if(player_keys[name].size() <= player) {
						player_keys[name].push_back(std::string((const char *)x));
					} else {
						player_keys[name][player] = std::string((const char *)x);
					}
					ss << "P(" << player << ") (" << name.c_str() << "," << x << " ) ";
				}
				free((void *)x);
				i++;
				if(i >= num_keys) {
					player++;
					i = 0;
				}
			}
		}


		OS::LogText(OS::ELogLevel_Info, "[%s] HB Keys: %s", OS::Address(m_address_info).ToString().c_str(), ss.str().c_str());
		ss.str("");

		m_server_info.m_keys = server_keys;
		m_server_info.m_player_keys = player_keys;
		m_server_info.m_team_keys 	= team_keys;

		//register gamename
		MM::MMPushRequest req;
		req.peer = this;
		if (m_server_info.m_game.gameid != 0) {
			if (m_server_pushed) {
				if (m_server_info.m_keys.find("statechanged") != m_server_info.m_keys.end() && atoi(m_server_info.m_keys["statechanged"].c_str()) == 2) {
					Delete();
					return;
				}
				req.server = &m_server_info;
				req.peer->IncRef();
				req.type = MM::EMMPushRequestType_UpdateServer;
				MM::m_task_pool->AddRequest(req);
			}
			else {
				OnGetGameInfo(m_server_info.m_game, (void *)1);
			}
		}
		else if(!m_sent_game_query){
			m_sent_game_query = true;
			req.peer->IncRef();
			req.extra = (void *)1;
			req.gamename = m_server_info.m_keys["gamename"];
			req.type = MM::EMMPushRequestType_GetGameInfoByGameName;
			MM::m_task_pool->AddRequest(req);
		}
	}
	void V2Peer::handle_available(char *buff, int len) {
		MM::MMPushRequest req;
		req.peer = this;
		req.peer->IncRef();
		req.extra = (void *)2;

		uint8_t *p= (uint8_t *)(buff + sizeof(m_instance_key));
		int buflen = len - sizeof(m_instance_key);
		char *str = (char *)BufferReadNTS((uint8_t **)&p, &buflen);

		if (str) {
			req.gamename = str;
			free((void *)str);
		}

		req.type = MM::EMMPushRequestType_GetGameInfoByGameName;
		MM::m_task_pool->AddRequest(req);
	}
	void V2Peer::OnGetGameInfo(OS::GameData game_info, void *extra) {
		if (extra == (void *)1) {
			m_server_info.m_game = game_info;
			if (m_server_info.m_game.gameid == 0) {
				send_error(true, "Game not found");
				return;
			}

			if (m_server_info.m_keys.find("statechanged") != m_server_info.m_keys.end() && atoi(m_server_info.m_keys["statechanged"].c_str()) == 2) {
				Delete();
				return;
			}

			if (!m_sent_challenge) {
				send_challenge();
			}

			//TODO: check if changed and only push changes
			if (m_server_pushed) {
				MM::MMPushRequest req;
				req.peer = this;
				req.server = &m_server_info;
				req.peer->IncRef();
				req.type = MM::EMMPushRequestType_UpdateServer;
				MM::m_task_pool->AddRequest(req);
			}
		}
		else if(extra == (void *)2) {
			char sendbuf[90] = { 0 };
			int outlen = 0;

			if (game_info.gameid == 0) {
				game_info.disabled_services = OS::QR2_GAME_UNAVAILABLE;
			}

			uint8_t *p = (uint8_t *)&sendbuf;
			BufferWriteByte((uint8_t**)&p, &outlen, PACKET_AVAILABLE);
			BufferWriteIntRE((uint8_t**)&p, &outlen, game_info.disabled_services);
			SendPacket((uint8_t *)&sendbuf, outlen);
			Delete();
		}
	}
	void V2Peer::send_error(bool die, const char *fmt, ...) {

		//XXX: make all these support const vars
		SendPacket((uint8_t*)fmt, strlen(fmt));
		if(die) {
			m_timeout_flag = false;
			Delete();
		}

		OS::LogText(OS::ELogLevel_Info, "[%s] Error:", OS::Address(m_address_info).ToString().c_str(), fmt);
	}
	void V2Peer::send_ping() {
		//check for timeout
		struct timeval current_time;


		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > QR2_PING_TIME) {

			char data[sizeof(m_instance_key) + 1];
			uint8_t *p = (uint8_t *)&data;
			int len = 0;

			gettimeofday(&m_last_ping, NULL);

			BufferWriteByte(&p, &len, PACKET_KEEPALIVE);
			BufferWriteData(&p, &len, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			SendPacket((uint8_t *)&data, len);
		}

	}
	void V2Peer::think() {
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > QR2_PING_TIME) {
			m_timeout_flag = true;
			send_error(true, "Timeout");
			
		}
	}

	void V2Peer::send_challenge() {
		char buff[100];
		uint8_t *p = (uint8_t *)buff;
		int blen = 0;

		memset(&m_challenge, 0, sizeof(m_challenge));
		gen_random((char *)&m_challenge,sizeof(m_challenge)-1);

		uint16_t *backend_flags = (uint16_t *)&m_challenge[13];
		*backend_flags &= ~QR2_OPTION_USE_QUERY_CHALLENGE;


		BufferWriteByte((uint8_t**)&p,&blen,PACKET_CHALLENGE);
		BufferWriteData((uint8_t **)&p, &blen, (uint8_t *)&m_instance_key, sizeof(m_instance_key));

		BufferWriteNTS((uint8_t**)&p,&blen,(uint8_t *)&m_challenge);


		SendPacket((uint8_t *)&buff, blen);
		m_sent_challenge = true;
	}
	void V2Peer::SendClientMessage(uint8_t *data, int data_len) {
		char buff[MAX_DATA_SIZE];
		uint8_t *p = (uint8_t *)buff;
		int blen = 0;
		uint32_t key = rand() % 100000 + 1;

		BufferWriteByte(&p, &blen, PACKET_CLIENT_MESSAGE);
		BufferWriteData((uint8_t **)&p, &blen, (uint8_t *)&m_instance_key, sizeof(m_instance_key));

		BufferWriteInt(&p, &blen, key);
		BufferWriteData(&p, &blen, data, data_len);
		SendPacket((uint8_t *)&buff, blen);
	}
	void V2Peer::Delete() {
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
