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

#include <stdarg.h>

#define CRYPTCHAL_LEN 10
#define SERVCHAL_LEN 25
namespace SB {
	V2Peer::V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver, address_info, sd, 2) {
		m_next_packet_send_msg = false;
		m_sent_crypt_header = false;
		m_sent_push_keys = false;
		m_game.secretkey[0] = 0;
		m_game.gameid = 0;
		m_game.gamename[0] = 0;
		m_last_list_req.m_from_game.gameid = 0;
		m_last_list_req.m_from_game.secretkey[0] = 0;
		m_last_list_req.m_from_game.gamename[0] = 0;
		m_in_message = false;
		m_got_game_pair = false;

		memset(&m_crypt_state,0,sizeof(m_crypt_state));
	}
	V2Peer::~V2Peer() {
	}
	void V2Peer::handle_packet(char *data, int len) {
		if(len == 0)
			return;

		uint8_t *buffer = (uint8_t *)data;
		void *end = (void *)((char *)data + len);
		int pos = len;

		uint8_t request_type = 0;

		bool break_flag = false;

		int idx = 0;
		while((buffer - (uint8_t*)data) < len) {
			uint32_t buff_len = Socket::htons(BufferReadShort(&buffer, &pos)); //length

			buff_len -= sizeof(uint16_t);

			bool improper_length = false;
			if(((char *)buffer + buff_len) > end) {
				improper_length = true;
			}

			request_type = BufferReadByte(&buffer, &pos);

			if(improper_length && request_type != SEND_MESSAGE_REQUEST) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Got improper length packet, type: %d", OS::Address(m_address_info).ToString().c_str(), request_type);
				m_delete_flag = true;
				break;
			}

			gettimeofday(&m_last_recv, NULL);

			/*if (m_game.secretkey[0] == 0 && request_type != SERVER_LIST_REQUEST && request_type != SEND_MESSAGE_REQUEST) { //only list req can define the game, anything else will result in a crash
				printf("No game key non req\n");
				return;
			}*/

			//TODO: get expected lengths for each packet type and test, prior to execution
			switch (request_type) {
				case SERVER_LIST_REQUEST:
					buffer = ProcessListRequest(buffer, pos);
					break;
				case KEEPALIVE_MESSAGE:
					buffer++; len--;
					break;
				case SEND_MESSAGE_REQUEST:
					buffer = ProcessSendMessage(buffer, pos);
					break_flag = true;
				break;
				case MAPLOOP_REQUEST:
					OS::LogText(OS::ELogLevel_Info, "[%s] Got map request", OS::Address(m_address_info).ToString().c_str(), request_type);
					break_flag = true;
				break;
				case PLAYERSEARCH_REQUEST:
					OS::LogText(OS::ELogLevel_Info, "[%s] Got playersearch request", OS::Address(m_address_info).ToString().c_str(), request_type);
					break_flag = true;
				break;
				case SERVER_INFO_REQUEST:
					buffer = ProcessInfoRequest(buffer, pos);
				break;
				default:
					OS::LogText(OS::ELogLevel_Info, "[%s] Got unknown packet type: %d", OS::Address(m_address_info).ToString().c_str(), request_type);
					break_flag = true;
					break;
			}
			pos = buffer - (uint8_t*)data;
			if(pos >= len || break_flag) {
				break;
			}
		}

	}
	MM::sServerListReq V2Peer::ParseListRequest(uint8_t **buffer, int remain) {
		uint32_t options;
		int buf_remain = remain;

		const char *from_gamename, *for_gamename, *filter, *field_list;

		MM::sServerListReq req;

		req.protocol_version = BufferReadByte(buffer, &buf_remain);
		req.encoding_version = BufferReadByte(buffer, &buf_remain);

		req.game_version = BufferReadInt(buffer, &buf_remain);


		for_gamename = (const char *)BufferReadNTS(buffer, &buf_remain);
		from_gamename = (const char *)BufferReadNTS(buffer, &buf_remain);

		if (!from_gamename) {
			send_error(true, "No from gamename");
			if(for_gamename)
				free((void *)for_gamename);
			return req;
		}

		if (!for_gamename) {
			send_error(true, "No for gamename");

			if (from_gamename)
				free((void *)from_gamename);

			return req;
		}

		BufferReadData(buffer, &buf_remain, (uint8_t*)&m_challenge, LIST_CHALLENGE_LEN);

		filter = (const char *)BufferReadNTS(buffer, &buf_remain);
		
		if(filter) {
			req.filter = filter;
		}
		field_list = (const char *)BufferReadNTS(buffer, &buf_remain);

		if (field_list) {
			req.field_list = OS::KeyStringToVector(field_list);
		}


		options = Socket::htonl(BufferReadInt(buffer, &buf_remain));

		req.send_groups = options & SEND_GROUPS;
		
		req.send_fields_for_all = options & SEND_FIELDS_FOR_ALL;
		req.no_server_list = options & NO_SERVER_LIST;
		req.push_updates = options & PUSH_UPDATES; //requesting updates,
		req.no_list_cache = options & NO_LIST_CACHE;
		

		if (options & ALTERNATE_SOURCE_IP) {
			req.source_ip = BufferReadInt(buffer, &buf_remain);
		}
		else {
			req.source_ip = 0;
		}
		if (options & LIMIT_RESULT_COUNT) {
			req.max_results = BufferReadInt(buffer, &buf_remain);
		}
		else {
			req.max_results = 0;
		}

		if (req.no_server_list && !req.field_list.size()) { //no requested field, send defaults
			std::map<std::string, uint8_t>::iterator it = req.m_for_game.push_keys.begin();
			while(it != req.m_for_game.push_keys.end()) {
				std::pair<std::string, uint8_t> pair = *it;
				req.field_list.push_back(pair.first);
				it++;
			}
		}


		req.m_for_gamename = for_gamename;
		req.m_from_gamename = from_gamename;

		OS::LogText(OS::ELogLevel_Info, "[%s] List Request: Version: (%d %d %d), gamenames: (%s) - (%s), fields: %s, filter: %s  is_group: %d, limit: %d, alt_src: %s, fields4all: %d, no_srv_list: %d, no_list_cache: %d, updates: %d, options: %d", OS::Address(m_address_info).ToString().c_str(), req.protocol_version, req.encoding_version, req.game_version, req.m_from_gamename.c_str(), req.m_for_gamename.c_str(), field_list, filter, req.send_groups, req.max_results, OS::Address(req.source_ip, 0).ToString().c_str(), req.send_fields_for_all, req.no_server_list, req.no_list_cache, req.push_updates, options);


		if(filter)
			free((void *)filter);
		if(field_list)
			free((void *)field_list);

		free((void *)for_gamename);
		free((void *)from_gamename);
		return req;

	}
	uint8_t *V2Peer::ProcessSendMessage(uint8_t *buffer, int remain) {
		uint8_t *p = buffer;
		int len = remain;

		m_send_msg_to.sin_addr.s_addr = (BufferReadInt(&p, &len));
		m_send_msg_to.sin_port = BufferReadShort(&p, &len);


		OS::LogText(OS::ELogLevel_Info, "[%s] Send msg to %s", OS::Address(m_address_info).ToString().c_str(), OS::Address(m_send_msg_to).ToString().c_str());
		if (len > 0) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Got msg length: %d", OS::Address(m_address_info).ToString().c_str(), len);
			const char *base64 = OS::BinToBase64Str((uint8_t *)p, len);
			MM::MMQueryRequest req;
			req.type = MM::EMMQueryRequestType_SubmitData;
			req.SubmitData.from = m_address_info;
			req.SubmitData.to = m_send_msg_to;
			req.SubmitData.base64 = base64;
			req.SubmitData.game = m_game;
			AddRequest(req);
			free((void *)base64);

		}
		else {
			m_next_packet_send_msg = true;
		}

		return p;

	}
	/*		
		TODO: save this with game data and load it from that
	*/
	uint8_t *V2Peer::WriteOptimizedField(struct MM::ServerListQuery servers, std::string field_name, uint8_t *buff, int *len, std::map<std::string, int> &field_types) {
		
		uint8_t *out = buff;
		uint8_t var = KEYTYPE_STRING;
		std::vector<MM::Server *>::iterator it = servers.list.begin();
		int highest_value = 0;
		bool is_digit = false;
		char * pEnd = NULL;
		while (it != servers.list.end()) {
			MM::Server *server = *it;
			std::string value = server->kvFields[field_name];

			int val = abs(strtol (value.c_str(),&pEnd,10));
			if(pEnd == NULL) {
				is_digit = true;
			}

			if(val > highest_value)
				highest_value = val;
			it++;
		}

		if(is_digit) {
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
				} else {
					var = KEYTYPE_STRING;
				}
			}
		}
		BufferWriteByte(&out, len, var); //key type, 0 = str, 1 = uint8, 2 = uint16 TODO: determine type
		BufferWriteNTS(&out, len, (uint8_t *)field_name.c_str());

		field_types[field_name] = var;

		return out;
	}
	void V2Peer::SendListQueryResp(struct MM::ServerListQuery servers, const MM::sServerListReq list_req, bool usepopularlist, bool send_fullkeys) {
		/*
		TODO: make support split packets
		*/

		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;

		if (list_req.source_ip != 0) {
			BufferWriteInt(&p, &len, Socket::htonl(list_req.source_ip));
		}
		else {
			
			BufferWriteInt(&p, &len, m_address_info.sin_addr.s_addr);
		}
		
		BufferWriteShort(&p, &len, Socket::htons(list_req.m_from_game.queryport));

		bool send_push_keys = false;
		bool no_keys = list_req.m_from_game.compatibility_flags & OS_COMPATIBILITY_FLAG_SBV2_FROMGAME_LIST_NOKEYS;

		if(!list_req.no_server_list) {
			BufferWriteByte(&p, &len, list_req.field_list.size());

			//send fields
			std::map<std::string, int> field_types;
			std::vector<std::string>::const_iterator field_it = list_req.field_list.begin();
			while (field_it != list_req.field_list.end()) {
				const std::string str = *field_it;
				p = WriteOptimizedField(servers, str, p, &len, field_types);
				field_it++;
			}

			//send popular string values list
			if (usepopularlist) {
				BufferWriteByte(&p, &len, list_req.m_for_game.popular_values.size());
				std::vector<std::string>::const_iterator it_v = list_req.m_for_game.popular_values.begin();
				while (it_v != list_req.m_for_game.popular_values.end()) {
					const std::string v = *it_v;
					BufferWriteNTS(&p, &len, (const uint8_t*)v.c_str());
					it_v++;
				}
			}
			else {
				BufferWriteByte(&p, &len, 0);
			}

			std::vector<MM::Server *>::iterator it = servers.list.begin();
			while (it != servers.list.end()) {
				MM::Server *server = *it;
					sendServerData(server, usepopularlist, false, servers.first_set ? &p : NULL, servers.first_set ? &len : NULL, false, &field_types, no_keys);
				it++;
			}

			if (!servers.first_set) {
				p = (uint8_t *)&buff;
				len = 0;
			}

			if (servers.list.empty() || servers.last_set) {
				//terminator
				BufferWriteByte((uint8_t **)&p, &len, 0x00);
				BufferWriteInt((uint8_t **)&p, &len, -1);
				if (!list_req.send_groups) {
					send_push_keys = true;
				}				
			}

		}

		gettimeofday(&m_last_recv, NULL); //prevent timeout during long lists

		if (len > 0) {
			SendPacket((uint8_t *)&buff, len, false);
		}

		if (!m_sent_push_keys && send_push_keys) {
			m_sent_push_keys = true;
			SendPushKeys();
		}

		if (servers.last_set) {
			m_in_message = false;
		}
	}
	void V2Peer::setupCryptHeader(uint8_t **dst, int *len) {
		//	memset(&options->cryptkey,0,sizeof(options->cryptkey));
		srand(time(NULL));
		uint32_t cryptlen = CRYPTCHAL_LEN;
		uint8_t cryptchal[CRYPTCHAL_LEN];
		uint32_t servchallen = SERVCHAL_LEN;
		uint8_t servchal[SERVCHAL_LEN];
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

		//combine our secret key, our challenge, and the server's challenge into a crypt key
		int seckeylen = (int)strlen(m_game.secretkey);
		char *seckey = (char *)&m_game.secretkey;
		for (uint32_t i = 0 ; i < servchallen ; i++)
		{
			m_challenge[(i *  seckey[i % seckeylen]) % LIST_CHALLENGE_LEN] ^= (char)((m_challenge[i % LIST_CHALLENGE_LEN] ^ servchal[i]) & 0xFF);
		}
		GOACryptInit(&(m_crypt_state), (unsigned char *)(&m_challenge), LIST_CHALLENGE_LEN);
	}
	void V2Peer::SendPacket(uint8_t *buff, int len, bool prepend_length) {
		if (m_delete_flag) {
			return;
		}
		uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		int header_len = 0;

		if (!m_sent_crypt_header && m_game.secretkey[0] != 0) {
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

		m_peer_stats.bytes_out += out_len;
		m_peer_stats.packets_out++;

		int c = 0;
		if((c = send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL)) < 0) {
			OS::LogText(OS::ELogLevel_Info, "[%s] Send Exit: %d", OS::Address(m_address_info).ToString().c_str(),c);
			m_delete_flag = true;
		}
	}
	uint8_t *V2Peer::ProcessListRequest(uint8_t *buffer, int remain) {
		MM::MMQueryRequest req;
		req.extra = this;

		req.req = this->ParseListRequest(&buffer, remain);

		OS::GameData old_gamedata[2];
		req.req.m_from_game = m_last_list_req.m_from_game;
		req.req.m_for_game = m_last_list_req.m_for_game;
		m_last_list_req = req.req;

		if (!m_got_game_pair || std::string(m_last_list_req.m_for_game.gamename).compare(req.req.m_for_gamename) != 0) {
			req.type = MM::EMMQueryRequestType_GetGameInfoPairByGameName;
			req.gamenames[0] = req.req.m_from_gamename;
			req.gamenames[1] = req.req.m_for_gamename;
			m_in_message = true;
			AddRequest(req);
		}
		else {
			this->OnRecievedGameInfoPair(m_game, m_last_list_req.m_for_game, NULL);
		}

		return buffer;
	}
	void V2Peer::OnRecievedGameInfo(const OS::GameData game_data, void *extra) {
		m_in_message = false;
	}
	void V2Peer::OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra) {
		m_in_message = false;
			
		MM::MMQueryRequest req;

		m_last_list_req.m_from_game = game_data_first;
		m_game = m_last_list_req.m_from_game;

		m_last_list_req.m_for_game = game_data_second;
		req.req = m_last_list_req;

		if (m_game.secretkey[0] == 0) {
			send_error(true, "Invalid source gamename");
			return;
		}

		if (req.req.m_for_game.secretkey[0] == 0) {
			send_error(true, "Invalid target gamename");
			return;
		}

		m_peer_stats.from_game = m_last_list_req.m_from_game;

		m_got_game_pair = true;

		if (!m_last_list_req.no_server_list) {
			if (m_last_list_req.send_groups) {
				req.type = MM::EMMQueryRequestType_GetGroups;
			}
			else {
				req.type = MM::EMMQueryRequestType_GetServers;
			}
			req.req.all_keys = true; //required for localip0, etc, TODO: find way that doesn't require retrieving full keys
			m_in_message = true;
			AddRequest(req);
		}
		else {
			//send empty server list
			MM::ServerListQuery servers;
			servers.requested_fields = req.req.field_list;
			SendListQueryResp(servers, req.req);
		}

		FlushPendingRequests();
	}
	uint8_t *V2Peer::ProcessInfoRequest(uint8_t *buffer, int remain) {
		uint8_t *p = (uint8_t *)buffer;
		int len = remain;
		OS::Address address;
		address.ip = BufferReadInt(&p, &len);
		address.port = BufferReadShort(&p, &len);

		sServerCache cache = FindServerByIP(address);

		MM::MMQueryRequest req;
		req.address = address;

		if(cache.key[0] != 0) {
			req.type = MM::EMMQueryRequestType_GetServerByKey;
			req.key = cache.key;
			OS::LogText(OS::ELogLevel_Info, "[%s] Get info request, cached %s %s", OS::Address(m_address_info).ToString().c_str(),cache.wan_address.ToString().c_str(),cache.key.c_str()) ;
			cache.full_keys = true;
		} else {
			req.type = MM::EMMQueryRequestType_GetServerByIP;
			req.address = address;
			OS::LogText(OS::ELogLevel_Info, "[%s] Get info request, non-cached %s", OS::Address(m_address_info).ToString().c_str(), req.address.ToString().c_str());
		}

		req.req.m_for_game = m_game;
		req.SubmitData.game = m_game;
		AddRequest(req);
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
		int len = 0;
		if (waiting_packet) {
			len = recv(m_sd, (char *)&buf, sizeof(buf), 0);
			if (len <= 0) {
				m_delete_flag = true;
				return;
			}

			m_peer_stats.packets_in++;
			m_peer_stats.bytes_in += len;

			if(m_next_packet_send_msg) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Got msg length: %d", OS::Address(m_address_info).ToString().c_str(), len);
				const char *base64 = OS::BinToBase64Str((uint8_t *)&buf, len);

				MM::MMQueryRequest req;
				req.type = MM::EMMQueryRequestType_SubmitData;
				req.SubmitData.from = m_address_info;
				req.SubmitData.to = m_send_msg_to;
				req.SubmitData.base64 = base64;
				req.SubmitData.game = m_game;
				AddRequest(req);
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
	void V2Peer::sendServerData(MM::Server *server, bool usepopularlist, bool push, uint8_t **out, int *out_len, bool full_keys, const std::map<std::string, int> *optimized_fields, bool no_keys) {
		char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
		uint8_t *p = (uint8_t *)&buf;

		cacheServer(server);

		if(out) {
			p = *out;
		}
		int len = 0;
		uint8_t flags = 0;
		int private_ip = 0;
		int private_port = 0;

		if (server->wan_address.port != server->game.queryport) {
			flags |= NONSTANDARD_PORT_FLAG;
		}
		int natneg_val = 0;
		if(server->kvFields.find("natneg") != server->kvFields.end()) {
			natneg_val = atoi(server->kvFields["natneg"].c_str());
		}
		else {
			flags |= CONNECT_NEGOTIATE_FLAG;
		}

		if(natneg_val == 0) {
			flags |= UNSOLICITED_UDP_FLAG;
		} else {
			flags |= CONNECT_NEGOTIATE_FLAG;
		}
		
		if(server->kvFields.find("localip0") != server->kvFields.end()) { //TODO: scan localips??
			int addr = inet_addr(server->kvFields["localip0"].c_str());
			flags |= PRIVATE_IP_FLAG;
			private_ip = addr;
		}
		
		if(server->kvFields.find("localport") != server->kvFields.end()) {
			int localport = atoi(server->kvFields["localport"].c_str());
			if(server->game.queryport != localport) {
				flags |= NONSTANDARD_PORT_FLAG;
				private_port = localport;
			}
		}
		
		if (!no_keys) {
			if (full_keys) {
				flags |= HAS_FULL_RULES_FLAG;
			}
			else {
				flags |= HAS_KEYS_FLAG;
			}
		}
		
		if(push) {
			BufferWriteByte(&p, &len, PUSH_SERVER_MESSAGE);
		}

		

		if (server->wan_address.GetIP() == -1) {
			return;
		}

		BufferWriteByte(&p, &len, flags); //flags
		BufferWriteInt(&p, &len, server->wan_address.ip); //ip

		if (flags & NONSTANDARD_PORT_FLAG) {
			BufferWriteShort(&p, &len, server->wan_address.port);
		}

		if (flags & PRIVATE_IP_FLAG) {
			BufferWriteInt(&p, &len, /*Socket::htonl*/(private_ip));
		}
		if (flags & NONSTANDARD_PRIVATE_PORT_FLAG) {
			BufferWriteShort(&p, &len, Socket::htons(private_port));
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
					if(push_it == server->game.popular_values.end()) { //not a popular value, must write directly
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
					else { //write popular string index
						BufferWriteByte(&p, &len, std::distance(server->game.popular_values.begin(), push_it));
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
		if(cache.key[0] == 0 || !m_last_list_req.push_updates || m_in_message) return;

		DeleteServerFromCacheByKey(server->key);

		BufferWriteByte(&p, &len, DELETE_SERVER_MESSAGE);
		BufferWriteInt(&p, &len, Socket::htonl(server->wan_address.ip));
		BufferWriteShort(&p, &len, Socket::htons(server->wan_address.port));
		SendPacket((uint8_t *)&buf, len, true);
	}
	void V2Peer::informNewServers(MM::Server *server) {
		sServerCache cache = FindServerByKey(server->key);
		if(cache.key[0] != 0 || !m_last_list_req.push_updates || m_in_message) return;
		if(server) {
			if(serverMatchesLastReq(server)) {
				cacheServer(server);
				sendServerData(server, false, true, NULL, NULL, true);
			}
		}
	}
	void V2Peer::informUpdateServers(MM::Server *server) {
		if(!m_last_list_req.push_updates || m_in_message) return;
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
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;
		std::map<std::string, uint8_t>::iterator it = m_last_list_req.m_from_game.push_keys.begin();

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
		if (m_delete_flag) {
			return;
		}
		if(die) {
			m_delete_flag = true;
		}
		va_list args;
		va_start(args, fmt);

		char send_str[1092]; //mtu size
		int len = vsprintf(send_str, fmt, args);
		send_str[len] = 0;


		m_peer_stats.bytes_out += len+1;
		m_peer_stats.packets_out++;
		if (send(m_sd, (const char *)&send_str, len+1, MSG_NOSIGNAL) < 0)
			m_delete_flag = true;

		OS::LogText(OS::ELogLevel_Info, "[%s] Got Error %s, fatal: %d", OS::Address(m_address_info).ToString().c_str(), send_str, die);

		va_end(args);
	}


	void V2Peer::OnRetrievedServers(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
		SendListQueryResp(results, request.req);
	}
	void V2Peer::OnRetrievedGroups(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
		SendListQueryResp(results, request.req);
	}
	void V2Peer::OnRetrievedServerInfo(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
		printf("OnRetrievedServerInfo: %d\n", results.list.size());
		if (results.list.size() == 0) return;
		MM::Server *server = results.list.front();
		if (server) {
			sendServerData(server, false, true, NULL, NULL, true);
			cacheServer(server, true);
		}
	}
}
