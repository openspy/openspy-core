#include "SBPeer.h"
#include "SBDriver.h"
#include "V2Peer.h"
#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <limits.h>
#include <sstream>
#include <algorithm>

#define CRYPTCHAL_LEN 10
#define SERVCHAL_LEN 25
namespace SB {
	V2Peer::V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver, address_info, sd) {
		m_next_packet_send_msg = false;
		m_sent_crypt_header = false;
	}
	V2Peer::~V2Peer() {
	}
	void V2Peer::handle_packet(char *data, int len) {
		if(len == 0)
			return;


		uint8_t *buffer = (uint8_t *)data;
		void *end = (void *)((void *)data + len);
		int pos = len;
		
		uint8_t request_type = 0;


		while((buffer - (uint8_t*)data) < len) {
			uint32_t buff_len = Socket::htons(BufferReadShort(&buffer, &pos)); //length

			buff_len -= sizeof(uint16_t);

			
			if(pos > len || ((void *)buffer + buff_len) > end) {
				break;
			}

			request_type = BufferReadByte(&buffer, &pos);
			printf("Got request %d\n", request_type);

			gettimeofday(&m_last_recv, NULL);

			if (m_game.secretkey[0] == 0 && request_type != SERVER_LIST_REQUEST) //only list req can define the game, anything else will result in a crash
				return;


 			//TODO: get expected lengths for each packet type and test, prior to execution
			switch (request_type) {
				case SERVER_LIST_REQUEST:
					printf("Got server list request\n");
					buffer = ProcessListRequset(buffer, pos);
					break;
				case KEEPALIVE_MESSAGE:
					printf("Got keepalive\n");
					break;
				case SEND_MESSAGE_REQUEST:
					buffer = ProcessSendMessage(buffer, pos);
					printf("msg request\n");
				break;
				case MAPLOOP_REQUEST:
					printf("maploop request\n");
				break;
				case PLAYERSEARCH_REQUEST:
					printf("playersearch request\n");
				break;
				case SERVER_INFO_REQUEST:
					printf("GOT INFO REQUEST!!\n");
					buffer = ProcessInfoRequest(buffer, pos);
				break;
			}
			pos = buffer - (uint8_t*)data;
		}

	}
	sServerListReq V2Peer::ParseListRequest(uint8_t **buffer, int remain) {
		uint8_t listversion, encodingversion;
		uint32_t fromgamever; //game version
		uint32_t srcip = 0;
		uint32_t maxServers = 0;
		uint32_t options;
		int buf_remain = remain;

		const char *from_gamename, *for_gamename, *filter, *field_list;

		sServerListReq req;

		listversion = BufferReadByte(buffer, &buf_remain);
		encodingversion = BufferReadByte(buffer, &buf_remain);

		fromgamever = BufferReadInt(buffer, &buf_remain);

		req.protocol_version = listversion;
		req.encoding_version = encodingversion;

		for_gamename = (const char *)BufferReadNTS(buffer, &buf_remain);
		from_gamename = (const char *)BufferReadNTS(buffer, &buf_remain);

		printf("Parse list (%s,%s)\n",from_gamename, for_gamename);

		if (from_gamename) {
			req.m_from_game = OS::GetGameByName(from_gamename);
			free((void *)from_gamename);
		} else {
			send_error(true, "No from gamename");
			return req;
		}

		if (for_gamename) {
			req.m_for_game = OS::GetGameByName(for_gamename);
			free((void *)for_gamename);
		} else {
			send_error(true, "No for gamename");
			return req;
		}
		

		BufferReadData(buffer, &buf_remain, (uint8_t*)&m_challenge, LIST_CHALLENGE_LEN);

		filter = (const char *)BufferReadNTS(buffer, &buf_remain);
		if(filter) {
			req.filter = filter;
			free((void *)filter);
		}
		field_list = (const char *)BufferReadNTS(buffer, &buf_remain);

		if (field_list) {
			req.field_list = OS::KeyStringToMap(field_list);
			free((void *)field_list);
		}


		options = Socket::htonl(BufferReadInt(buffer, &buf_remain));

		req.send_groups = options & SEND_GROUPS;
		req.push_updates = options & PUSH_UPDATES; //requesting updates, 
		req.no_server_list = options & NO_SERVER_LIST;

		if (options & ALTERNATE_SOURCE_IP) {
			req.source_ip = BufferReadInt(buffer, &buf_remain);
		}
		if (options & LIMIT_RESULT_COUNT) {
			req.max_results = BufferReadInt(buffer, &buf_remain);
		}

		if (req.no_server_list && !req.field_list.size()) { //no requested field, send defaults
			std::map<std::string, uint8_t>::iterator it = req.m_for_game.push_keys.begin();
			while(it != req.m_for_game.push_keys.end()) {
				std::pair<std::string, uint8_t> pair = *it;
				req.field_list.push_back(pair.first);
				it++;
			}
		}
		

		return req;

	}
	uint8_t *V2Peer::ProcessSendMessage(uint8_t *buffer, int remain) {
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = buffer;
		int len = remain;

		
		m_send_msg_to.sin_addr.s_addr = (BufferReadInt(&p, &len));
		m_send_msg_to.sin_port = Socket::htons(BufferReadShort(&p, &len));
		const char *ipinput = Socket::inet_ntoa(m_send_msg_to.sin_addr);

		m_next_packet_send_msg = true;

		return p;

	}
	uint8_t *V2Peer::WriteOptimizedField(struct MM::ServerListQuery servers, std::string field_name, uint8_t *buff, int *len, std::map<std::string, int> &field_types) {
		uint8_t *out = buff;
		uint8_t var = KEYTYPE_STRING;
		std::vector<MM::Server *>::iterator it = servers.list.begin();
		int highest_value = 0;
		while (it != servers.list.end()) {
			MM::Server *server = *it;
			std::string value = server->kvFields[field_name];
			int val = abs(atoi(value.c_str()));;
			if(val > highest_value)
				highest_value = val;
			it++;
		}

		if(highest_value != 0) {
			std::vector<std::string>::iterator push_it;
			std::ostringstream s;
			s << highest_value;

			/*
				Check if the highest value is a popular value, if so this will be more efficent
			*/
			push_it = std::find(m_last_list_req.m_for_game.popular_values.begin(),m_last_list_req.m_for_game.popular_values.end(),s.str());
			if(push_it == m_last_list_req.m_for_game.popular_values.end()) {
				if(highest_value <= SCHAR_MAX) {
					var = KEYTYPE_BYTE;
				} else if(highest_value <= SHRT_MAX) {
					var = KEYTYPE_SHORT;
				}
			}
		}
		BufferWriteByte(&out, len, var); //key type, 0 = str, 1 = uint8, 2 = uint16 TODO: determine type
		BufferWriteNTS(&out, len, (uint8_t *)field_name.c_str());

		field_types[field_name] = var;

		return out;
	}
	void V2Peer::SendListQueryResp(struct MM::ServerListQuery servers, sServerListReq *list_req, bool usepopularlist, bool send_fullkeys) {
		/*
		TODO: make support split packets
		*/
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;

		BufferWriteInt(&p, &len, m_address_info.sin_addr.s_addr);
		BufferWriteShort(&p, &len, Socket::htons(list_req->m_from_game.queryport));

		if(!list_req->no_server_list) {
			BufferWriteByte(&p, &len, list_req->field_list.size());

			//send fields
			std::map<std::string, int> field_types;
			std::vector<std::string>::iterator field_it = list_req->field_list.begin();
			while (field_it != list_req->field_list.end()) {
				std::string str = *field_it;
				p = WriteOptimizedField(servers, str, p, &len, field_types);
				field_it++;
			}
			//send popular string values list
			if(usepopularlist) {
				BufferWriteByte(&p, &len, list_req->m_for_game.popular_values.size());
				std::vector<std::string>::iterator it_v = list_req->m_for_game.popular_values.begin();
				while(it_v != list_req->m_for_game.popular_values.end()) {
					std::string v = *it_v;
					BufferWriteNTS(&p, &len, (const uint8_t*)v.c_str());
					it_v++;
				}
			} else {
				BufferWriteByte(&p, &len, 0);	
			}
			

			
			std::vector<MM::Server *>::iterator it = servers.list.begin();
			while (it != servers.list.end()) {
				MM::Server *server = *it;
				sendServerData(server, true, false, &p, &len, false, &field_types);
				it++;
			}

			//terminator
			BufferWriteByte((uint8_t **)&p, &len, 0x00);
			BufferWriteInt((uint8_t **)&p, &len, -1);
		}

		SendPacket((uint8_t *)&buff, len, false);
	}
	void V2Peer::setupCryptHeader(uint8_t **dst, int *len) {
		//	memset(&options->cryptkey,0,sizeof(options->cryptkey));
		srand(time(NULL));
		uint32_t cryptlen = CRYPTCHAL_LEN;
		uint8_t cryptchal[CRYPTCHAL_LEN];
		uint32_t servchallen = SERVCHAL_LEN;
		uint8_t servchal[SERVCHAL_LEN];
		int headerLen = (servchallen + cryptlen) + (sizeof(uint8_t) * 2);
		uint16_t *backendflags = (uint16_t *)(&cryptchal);
		for (uint32_t i = 0; i<cryptlen; i++) {
			cryptchal[i] = (uint8_t)rand();
		}
		*backendflags = 0;
		for (uint32_t i = 0; i<servchallen; i++) {
			servchal[i] = (uint8_t)rand();
		}

		BufferWriteByte(dst, len, cryptlen ^ 0xEC);
		BufferWriteData(dst, len, (uint8_t *)&cryptchal, cryptlen);
		BufferWriteByte(dst, len, servchallen ^ 0xEA);
		BufferWriteData(dst, len, (uint8_t *)&servchal, servchallen);

		printf("setup crypt for key %s %s\n",m_game.gamename,m_game.secretkey);


		//combine our secret key, our challenge, and the server's challenge into a crypt key
		int seckeylen = (int)strlen(m_game.secretkey);
		char *seckey = (char *)&m_game.secretkey;
		for (int i = 0 ; i < servchallen ; i++)
		{
			m_challenge[(i *  seckey[i % seckeylen]) % LIST_CHALLENGE_LEN] ^= (char)((m_challenge[i % LIST_CHALLENGE_LEN] ^ servchal[i]) & 0xFF);
		}
		GOACryptInit(&(m_crypt_state), (unsigned char *)(&m_challenge), LIST_CHALLENGE_LEN);
	}
	void V2Peer::SendPacket(uint8_t *buff, int len, bool prepend_length) {
		uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		int header_len = 0;
		if (!m_sent_crypt_header) {
			//this is actually part of the main key list, not to be sent on each packet
			setupCryptHeader(&p, &out_len);
 			header_len = out_len;
			m_sent_crypt_header = true;
		}
		if(prepend_length) {
			BufferWriteShort(&p, &out_len, Socket::htons(len + sizeof(uint16_t)));
		}
		BufferWriteData(&p, &out_len, buff, len);
		GOAEncrypt(&m_crypt_state, ((unsigned char *)&out_buff) + header_len, out_len - header_len);
		if(send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL) < 0)
			m_delete_flag = true;
	}
	uint8_t *V2Peer::ProcessListRequset(uint8_t *buffer, int remain) {

		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p;
		uint32_t len = 0;

		sServerListReq list_req = this->ParseListRequest(&buffer, remain);

		m_last_list_req = list_req;

		m_game = list_req.m_from_game;


		printf("Key req: %s | %s | %s\n",list_req.m_from_game.gamename,list_req.m_for_game.gamename, m_game.gamename);

		if (m_game.secretkey[0] == 0)
			return buffer;

		MM::ServerListQuery servers;
		servers.requested_fields = list_req.field_list;

		 if(!list_req.no_server_list) {
			if (list_req.send_groups) {
				servers = MM::GetGroups(&list_req);
			}
			else {
				servers = MM::GetServers(&list_req);
				printf("Got %d servers\n",servers.list.size());
			}
		}
		SendListQueryResp(servers, &list_req);
		FreeServerListQuery(&servers);
		if(!list_req.send_groups && list_req.no_server_list)
			SendPushKeys();

		return buffer;
	}
	uint8_t *V2Peer::ProcessInfoRequest(uint8_t *buffer, int remain) {
		uint8_t *p = (uint8_t *)buffer;
		int len = remain;
		OS::Address address;
		address.ip = Socket::htonl(BufferReadInt(&p, &len));
		address.port = Socket::htons(BufferReadShort(&p, &len));
		sServerCache cache = FindServerByIP(address);
		MM::Server *server = NULL;
		if(cache.key[0] != 0) {
			server = MM::GetServerByKey(cache.key);
			cache.full_keys = true;
		} else {
			server = MM::GetServerByIP(address, m_last_list_req.m_for_game);
		}
			if(server) {
				cacheServer(server, true);
			}
		if(server) {
			sendServerData(server, false, true, NULL, NULL, true);
		}
		return p;
	}
	void V2Peer::send_ping() {
		//check for timeout
		struct timeval current_time;
		
		uint8_t buff[10];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > SB_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			BufferWriteByte(&p, &len, KEEPALIVE_MESSAGE);

			//embed response into reply, as SB client just sends same data back
			BufferWriteByte(&p, &len, KEEPALIVE_REPLY);
			SendPacket((uint8_t *)&buff, len, true);
		}
		
	}
	void V2Peer::think(bool waiting_packet) {
		char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len;
		if (waiting_packet) {
			len = recv(m_sd, (char *)&buf, sizeof(buf), 0);
			if(m_next_packet_send_msg) {
				const char *base64 = OS::BinToBase64Str((uint8_t *)&buf, len);
				MM::SubmitData(base64, &m_address_info, &m_send_msg_to, &m_last_list_req.m_for_game);

				free((void *)base64);
				m_next_packet_send_msg = false;
			} else {
				this->handle_packet(buf, len);
			}
		}
		
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > SB_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
		} else if(len == 0 && waiting_packet) {
			m_delete_flag = true;
		}
	}
	void V2Peer::sendServerData(MM::Server *server, bool usepopularlist, bool push, uint8_t **out, int *out_len, bool full_keys, const std::map<std::string, int> *optimized_fields) {
		char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
		uint8_t *p = (uint8_t *)&buf;

		cacheServer(server);

		if(out) {
			p = *out;
		}
		int len = 0;
		uint8_t flags = 0;
		if(full_keys) {
			flags |= HAS_FULL_RULES_FLAG;
		} else {
			flags |= HAS_KEYS_FLAG;
		}

		if (server->wan_address.port != server->game.queryport) {
			flags |= NONSTANDARD_PORT_FLAG;
		}
		if(push) {
			BufferWriteByte(&p, &len, PUSH_SERVER_MESSAGE);	
		}
		
		BufferWriteByte(&p, &len, flags); //flags
		BufferWriteInt(&p, &len, Socket::htonl(server->wan_address.ip)); //ip

		if (flags & NONSTANDARD_PORT_FLAG) {
			BufferWriteShort(&p, &len, Socket::htons(server->wan_address.port));
		}
		
		if (flags & PRIVATE_IP_FLAG) {
			BufferWriteInt(&p, &len, Socket::htonl(server->lan_address.ip));
		}
		if (flags & NONSTANDARD_PRIVATE_PORT_FLAG) {
			BufferWriteShort(&p, &len, Socket::htons(server->lan_address.port));
		}

		if(flags & HAS_KEYS_FLAG) {
			std::vector<std::string>::iterator tok_it = m_last_list_req.field_list.begin();
			while (tok_it != m_last_list_req.field_list.end()) {
				std::vector<std::string>::iterator push_it = server->game.popular_values.end();
				std::string value;
				if(server->kvFields.find((*tok_it)) != server->kvFields.end()) {
					value = server->kvFields[*tok_it].c_str();
					push_it = std::find(server->game.popular_values.begin(),server->game.popular_values.end(),value);
				}

				/*
					Write values
				*/
				if(usepopularlist) {
					if(push_it == server->game.popular_values.end()) {
						int type = KEYTYPE_STRING;
						if(optimized_fields) {
							if(optimized_fields->find(*tok_it) != optimized_fields->end())
								type = (*(optimized_fields->find(*tok_it))).second;
						}
						/*
							This optimziation works on HAS_KEY_FLAG only
						*/
						if(type == KEYTYPE_STRING)
							BufferWriteByte(&p, &len, 0xFF); //string index, -1 = no index
						if (value.length() > 0) {

							switch(type) {
								case KEYTYPE_BYTE:
									BufferWriteByte(&p, &len, (atoi(value.c_str())));
									break;
								case KEYTYPE_SHORT:
									BufferWriteShort(&p, &len, Socket::htons(atoi(value.c_str())));
									break;
								case KEYTYPE_STRING:
									BufferWriteNTS(&p, &len, (uint8_t*)value.c_str());
									break;
								break;	
							}
							
						}
						else {
							BufferWriteByte(&p, &len, 0);
						}
					}
					else {
						BufferWriteByte(&p, &len, std::distance(server->game.popular_values.begin(), push_it));
						//printf("Writing idx %d for (%s)%s\n", std::distance(server->game.popular_values.begin(), push_it), (*tok_it).c_str(), value.c_str());
					}
				}
				tok_it++;
			}
		}

		if(flags & HAS_FULL_RULES_FLAG) {
		
			//std::map<int, std::map<std::string, std::string> > kvPlayers;
			std::map<int, std::map<std::string, std::string> >::iterator it = server->kvPlayers.begin();
			std::ostringstream s;
			std::pair<int, std::map<std::string, std::string> > index_mappair;
			std::map<std::string, std::string>::iterator it2;
			std::pair<std::string, std::string> kvp;


			std::map<std::string, std::string>::iterator field_it = server->kvFields.begin();
			while(field_it != server->kvFields.end()) {
				std::pair<std::string, std::string> pair = *field_it;
				BufferWriteNTS(&p, &len, (const uint8_t *)pair.first.c_str());
				BufferWriteNTS(&p, &len, (const uint8_t *)pair.second.c_str());				
				field_it++;
			}
			

			it = server->kvPlayers.begin();
			while(it != server->kvPlayers.end()) {
				index_mappair = *it;
				it2 = index_mappair.second.begin();
				while(it2 != index_mappair.second.end()) {
					kvp = *it2;
					s << kvp.first << index_mappair.first;
					BufferWriteNTS(&p, &len, (const uint8_t *)s.str().c_str());
					BufferWriteNTS(&p, &len, (const uint8_t *)kvp.second.c_str());
					s.str("");
					it2++;
				}
				it++;
			}

			it = server->kvTeams.begin();
			while(it != server->kvTeams.end()) {
				index_mappair = *it;
				it2 = index_mappair.second.begin();
				while(it2 != index_mappair.second.end()) {
					kvp = *it2;
					s << kvp.first << index_mappair.first;
					BufferWriteNTS(&p, &len, (const uint8_t *)s.str().c_str());
					BufferWriteNTS(&p, &len, (const uint8_t *)kvp.second.c_str());
					s.str("");
					it2++;
				}
				it++;
			}
		}

		if(out) {
			*out = p;
			*out_len += len;
		} else {
			SendPacket((uint8_t *)&buf, len, push);
		}
	}
	void V2Peer::informDeleteServers(MM::Server *server) {
		char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
		uint8_t *p = (uint8_t *)&buf;
		int len = 0;

		sServerCache cache = FindServerByKey(server->key);
		if(cache.key[0] == 0) return;

		DeleteServerFromCacheByKey(server->key);

		BufferWriteByte(&p, &len, DELETE_SERVER_MESSAGE);
		BufferWriteInt(&p, &len, Socket::htonl(server->wan_address.ip));
		BufferWriteShort(&p, &len, Socket::htons(server->wan_address.port));
		SendPacket((uint8_t *)&buf, len, true);
	}
	void V2Peer::informNewServers(MM::Server *server) {
		sServerCache cache = FindServerByKey(server->key);
		if(cache.key[0] != 0) return;
		if(server) {
			if(serverMatchesLastReq(server)) {
				cacheServer(server);
				sendServerData(server, false, true, NULL, NULL, true);
			}
		}
	}
	void V2Peer::informUpdateServers(MM::Server *server) {
		sServerCache cache = FindServerByKey(server->key);

		//client never recieved server notification, add to cache and send anyways, as it will be registered as a new server by the SB SDK
		if(cache.key[0] == 0 && server && serverMatchesLastReq(server)) {
			cacheServer(server);
		}
		if(server && serverMatchesLastReq(server)) {
			sendServerData(server, false, true, NULL, NULL, true);
		}
	}

	void V2Peer::SendPushKeys() {
		//list_req->m_from_game
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;
		std::map<std::string, uint8_t>::iterator it = m_last_list_req.m_from_game.push_keys.begin();
		//m_last_list_req.m_from_game.push_keys
		BufferWriteByte(&p, &len, m_last_list_req.m_from_game.push_keys.size());
		while(it != m_last_list_req.m_from_game.push_keys.end()) {
			std::pair<std::string, uint8_t> pair = *it;
			BufferWriteByte(&p, &len, pair.second);
			BufferWriteNTS(&p, &len, (const uint8_t*)pair.first.c_str());
			it++;
		}
		SendPacket((uint8_t *)&buff, len, true);
	}
	void V2Peer::send_error(bool die, const char *fmt, ...) {
		if(die) {
			m_delete_flag = true;
		}
		printf("SBV2 die: %s\n", fmt);
	}
}