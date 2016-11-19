#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
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
	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		m_address_info = *address_info;
		m_recv_instance_key = false;
		m_server_pushed = false;
		m_sent_challenge = false;
		m_delete_flag = false;
		m_timeout_flag = false;
		memset(&m_challenge, 0, sizeof(m_challenge));
	}
	Peer::~Peer() {
		if(m_server_pushed) {
			MM::DeleteServer(&m_server_info);
		}
	}

	void Peer::SendPacket(const uint8_t *buff, int len) {
		char sendbuf[MAX_DATA_SIZE + 1];
		uint8_t *p = (uint8_t *)&sendbuf;
		int blen = 0;
		BufferWriteByte((uint8_t**)&p,&blen,QR_MAGIC_1);
		BufferWriteByte((uint8_t**)&p,&blen,QR_MAGIC_2);
		BufferWriteData((uint8_t**)&p,&blen,buff, len);
		sendto(m_sd,(char *)&sendbuf,blen,0,(struct sockaddr *)&m_address_info, sizeof(sockaddr_in));
	}
	void Peer::handle_packet(char *recvbuf, int len) {
		printf("QR2 handle packet %d\n", len);
		

		uint8_t *buff = (uint8_t *)recvbuf;
		int buflen = len;

		uint8_t type = BufferReadByte((uint8_t**)&buff,&buflen);

		if(m_recv_instance_key) {
			uint8_t instance_key[REQUEST_KEY_LEN];
			BufferReadData((uint8_t **)&buff, (int *)&len, (uint8_t *)&instance_key, sizeof(instance_key));
			if(memcmp((uint8_t *)&instance_key, (uint8_t *)&m_instance_key, sizeof(instance_key)) != 0) {
				printf("possible spoofed packet\n"); //TODO: send echo and capture response
				return;
			}
		}

		gettimeofday(&m_last_recv, NULL);

		printf("QR got type: %02x || %d\n",type, len);
		switch(type) {
			case PACKET_HEARTBEAT:
				printf("Got qr heartbeat\n");
				handle_heartbeat((char *)buff, buflen);
			break;
			case PACKET_CHALLENGE:
				printf("Got qr challenge\n");
				handle_challenge((char *)buff, buflen);
			break;
			case PACKET_KEEPALIVE:
				handle_keepalive((char *)buff, buflen);
			break;
			case PACKET_CLIENT_MESSAGE_ACK:
				printf("Got client msg ack %d\n",buflen);
			break;
		}

		if(!m_sent_challenge) {
			send_challenge();
		}
	}
	void Peer::handle_challenge(char *buff, int len) {
		char challenge_resp[90] = { 0 };
		int outlen = 0;
		uint8_t *p = (uint8_t *)challenge_resp;
		gsseckey((unsigned char *)&challenge_resp, (unsigned char *)&m_challenge, (unsigned char *)&m_server_info.m_game.secretkey, 0);
		printf("%s == %s\n",buff,challenge_resp);
		if(strcmp(buff,challenge_resp) == 0) { //matching challenge
			BufferWriteByte((uint8_t**)&p, &outlen,PACKET_CLIENT_REGISTERED);
			BufferWriteData((uint8_t **)&p, &outlen, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			SendPacket((uint8_t *)&challenge_resp, outlen);
			if(m_sent_challenge) {
				MM::PushServer(&m_server_info, true);
				m_server_pushed = true;
			}
			m_sent_challenge = true;
		}
	}
	void Peer::handle_keepalive(char *buff, int len) {
		uint32_t *key = (uint32_t *)buff;
		if(*key == *(uint32_t *)&m_instance_key)
			SendPacket((uint8_t*)buff, len);
	}
	void Peer::handle_heartbeat(char *buff, int len) {
		int i = 0;
		uint8_t *x;

		std::map<std::string, std::string> server_keys;
		std::map<std::string, std::vector<std::string> > player_keys;
		std::map<std::string, std::vector<std::string> > team_keys;
		
		std::string key, value;

		if(!m_recv_instance_key) {
			BufferReadData((uint8_t **)&buff, (int *)&len, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			m_recv_instance_key = true;
		}
		while((buff[0] != 0 && len > 0) || (i%2 != 0)) {

			x = BufferReadNTS((uint8_t **)&buff,&len);
			
			if(i%2 == 0) {
				key = std::string((char *)x);
			} else {
				value = std::string((char *)x);
			}

			if(value.length() > 0) {
				server_keys[key] = value;
				value = std::string();
			}
			free((void *)x);
			i++;
		}
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
			int player=0,num_keys_t = num_keys,num_values_t = num_values*num_keys;
			i = 0;
			BufferReadByte((uint8_t**)&buff,&len);
			while(num_values_t--) {
				std::string name = nameValueList.at(i);
				bool is_team = false;
				x = BufferReadNTS((uint8_t **)&buff,&len);	

				if(isTeamString(name.c_str())) {
					team_keys[name].push_back(std::string((const char *)x));
				} else {
					player_keys[name].push_back(std::string((const char *)x));
				}
				free((void *)x);
				i++;
				if(i >= num_keys)
					i = 0;
			}
		}

		m_server_info.m_keys = server_keys;
		m_server_info.m_player_keys = player_keys;
		m_server_info.m_team_keys 	= team_keys;

		//register gamename
		m_server_info.m_game = OS::GetGameByName(m_server_info.m_keys["gamename"].c_str());

		if(m_server_info.m_game.gameid == 0) {
			send_error("Game not found", true);
			return;
		}

		m_server_info.m_address.port = Socket::htons(m_address_info.sin_port);
		m_server_info.m_address.ip = Socket::htonl(m_address_info.sin_addr.s_addr);

		//TODO: check if changed and only push changes
		if(m_server_pushed) {
			MM::UpdateServer(&m_server_info);
		}
	}
	void Peer::send_error(const char *msg, bool die) {

		//XXX: make all these support const vars
		SendPacket((uint8_t*)msg, strlen(msg));
		if(die) {
			m_timeout_flag = false;
			m_delete_flag = true;
		}
	}
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > QR_PING_TIME) {

			char data[sizeof(m_instance_key) + 1];
			uint8_t *p = (uint8_t *)&data;
			int len = 0;

			gettimeofday(&m_last_ping, NULL);
			
			BufferWriteByte(&p, &len, PACKET_KEEPALIVE);
			BufferWriteData(&p, &len, (uint8_t *)&m_instance_key, sizeof(m_instance_key));
			SendPacket((uint8_t *)&data, len);
		}
		
	}
	void Peer::think(bool waiting_packet) {
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > QR_PING_TIME) {
			m_delete_flag = true;
			m_timeout_flag = true;
		}
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


	void Peer::send_challenge() {
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
	void Peer::SendClientMessage(uint8_t *data, int data_len) {
		char buff[MAX_DATA_SIZE];
		uint8_t *p = (uint8_t *)buff;
		int blen = 0;
		uint32_t key = rand() % 100000 + 1;	

		BufferWriteByte(&p, &blen, PACKET_CLIENT_MESSAGE);
		BufferWriteInt(&p, &blen, key);

		BufferWriteData(&p, &blen, data, data_len);
		SendPacket((uint8_t *)&buff, blen);
		
	}
}