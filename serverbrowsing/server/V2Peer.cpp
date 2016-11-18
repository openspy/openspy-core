#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <sstream>
#include "SBDriver.h"
#include "SBPeer.h"
#include "V2Peer.h"

#define CRYPTCHAL_LEN 10
#define SERVCHAL_LEN 25

namespace SB {
	V2Peer::V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver,address_info, sd) {
		m_sent_crypt_header = false;

		m_next_packet_send_msg = false;
		printf("V2 peer\n");
	}
	V2Peer::~V2Peer() {
		printf("Peer delete\n");
	}
	void V2Peer::handle_packet(char *data, int len) {
		if(len == 0)
			return;


		uint8_t *buffer = (uint8_t *)data;
		int pos = len;
		
		uint8_t request_type = 0;

		BufferReadShort(&buffer, &pos);

		request_type = BufferReadByte(&buffer, &pos);

		gettimeofday(&m_last_recv, NULL);

		switch (request_type) {
			case SERVER_LIST_REQUEST:
				ProcessListRequset(buffer, pos);
				break;
			case KEEPALIVE_MESSAGE:
				break;
			case SEND_MESSAGE_REQUEST:
				ProcessSendMessage(buffer, pos);
			break;
			case SERVER_INFO_REQUEST:
				ProcessInfoRequest(buffer, pos);
			break;
			case MAPLOOP_REQUEST:
				printf("maploop request\n");
			break;
			case PLAYERSEARCH_REQUEST:
				printf("playersearch request\n");
			break;
		}

	}
	sServerListReq V2Peer::ParseListRequest(uint8_t *buffer, int remain) {
		uint8_t listversion, encodingversion;
		uint32_t fromgamever; //game version
		uint32_t srcip = 0;
		uint32_t maxServers = 0;
		uint32_t options;
		int buf_remain = remain;

		const char *from_gamename, *for_gamename, *filter, *field_list;

		sServerListReq req;

		listversion = BufferReadByte(&buffer, &buf_remain);
		encodingversion = BufferReadByte(&buffer, &buf_remain);

		fromgamever = BufferReadInt(&buffer, &buf_remain);

		req.protocol_version = listversion;
		req.encoding_version = encodingversion;

		from_gamename = (const char *)BufferReadNTS(&buffer, &buf_remain);
		for_gamename = (const char *)BufferReadNTS(&buffer, &buf_remain);

		if (from_gamename) {
			req.m_from_game = OS::GetGameByName(from_gamename);
			free((void *)from_gamename);
		}

		if (from_gamename) {
			req.m_for_game = OS::GetGameByName(for_gamename);
			free((void *)for_gamename);
		}
		

		BufferReadData(&buffer, &buf_remain, (uint8_t*)&m_challenge, LIST_CHALLENGE_LEN);

		req.filter = (const char *)BufferReadNTS(&buffer, &buf_remain);
		field_list = (const char *)BufferReadNTS(&buffer, &buf_remain);

		if (field_list) {
			req.field_list = OS::KeyStringToMap(field_list);
			free((void *)field_list);
		}
		if (!req.field_list.size())
			req.field_list = OS::KeyStringToMap("\\hostname\\numplayers\\maxplayers\\password\\region");

		options = BufferReadIntRE(&buffer, &buf_remain);

		req.send_groups = options & SEND_GROUPS;
		req.push_updates = options & PUSH_UPDATES; //requesting updates, 
		req.no_server_list = options & NO_SERVER_LIST;

		if (options & ALTERNATE_SOURCE_IP) {
			req.source_ip = BufferReadInt(&buffer, &buf_remain);
		}
		if (options & LIMIT_RESULT_COUNT) {
			req.max_results = BufferReadInt(&buffer, &buf_remain);
		}
		

		return req;

	}
	void V2Peer::ProcessSendMessage(uint8_t *buffer, int remain) {
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = buffer;
		int len = remain;

		
		m_send_msg_to.sin_addr.s_addr = (BufferReadInt(&p, &len));
		m_send_msg_to.sin_port = Socket::htons(BufferReadShort(&p, &len));
		const char *ipinput = Socket::inet_ntoa(m_send_msg_to.sin_addr);

		m_next_packet_send_msg = true;

	}
	void V2Peer::SendListQueryResp(struct MM::ServerListQuery servers, sServerListReq *list_req, bool usepopularlist, bool send_fullkeys) {
		/*
		TODO: make support split packets
		*/
		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p = (uint8_t *)&buff;
		int len = 0;

		BufferWriteInt(&p, &len, m_address_info.sin_addr.s_addr);
		BufferWriteShortRE(&p, &len, list_req->m_from_game.queryport);

		BufferWriteByte(&p, &len, list_req->field_list.size());

		//send fields
		std::vector<std::string>::iterator field_it = list_req->field_list.begin();
		while (field_it != list_req->field_list.end()) {
			std::string str = *field_it;
			BufferWriteByte(&p, &len, 0x00); //key type, 0 = str, 1 = uint8, 2 = uint16 TODO: determine type
			BufferWriteNTS(&p, &len, (uint8_t *)str.c_str());
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
			sendServerData(server, true, false, &p, &len);
			it++;
		}

		//terminator
		BufferWriteByte((uint8_t **)&p, &len, 0x00);
		BufferWriteInt((uint8_t **)&p, &len, -1);

		SendPacket((uint8_t *)&buff, len, false);
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
			BufferWriteShortRE(&p, &out_len, len + sizeof(uint16_t));
		}
		BufferWriteData(&p, &out_len, buff, len);
		enctypex_func6e((unsigned char *)&encxkeyb, ((unsigned char *)&out_buff) + header_len, out_len - header_len);
		if(send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL) < 0)
			m_delete_flag = true;
	}
	void V2Peer::ProcessListRequset(uint8_t *buffer, int remain) {

		uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
		uint8_t *p;
		uint32_t len = 0;

		sServerListReq list_req = this->ParseListRequest(buffer, remain);

		m_last_list_req = list_req;

		m_game = list_req.m_from_game;

		if (m_game.secretkey[0] == 0)
			return;

		MM::ServerListQuery servers;
		servers.requested_fields = list_req.field_list;

		if(list_req.no_server_list) {

		} else {
			if (list_req.send_groups) {
				servers = MM::GetGroups(&list_req);
			}
			else {
				servers = MM::GetServers(&list_req);
			}
		}
		SendListQueryResp(servers, &list_req);

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
		}
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
		enctypex_funcx((unsigned char *)&encxkeyb, (unsigned char *)&m_game.secretkey, (unsigned char *)m_challenge, (unsigned char *)&servchal, servchallen);
	}


	void V2Peer::informDeleteServers(MM::ServerListQuery servers) {

		std::vector<MM::Server *>::iterator it = servers.list.begin();
		while(it != servers.list.end()) {
			MM::Server *s = *it;
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			uint8_t *p = (uint8_t *)&buf;
			int len = 0;
			
			if(FindServerByIP(s->wan_address).key[0] != 0) {
				DeleteServerFromCacheByIP(s->wan_address);
				if(serverMatchesLastReq(s)) {
					BufferWriteByte(&p, &len, DELETE_SERVER_MESSAGE);
					BufferWriteInt(&p, &len, Socket::htonl(s->wan_address.ip));
					BufferWriteShort(&p, &len, Socket::htons(s->wan_address.port));
					SendPacket((uint8_t *)&buf, len, true);
				}
			}
			it++;
		}
	}
	void V2Peer::sendServerData(MM::Server *server, bool usepopularlist, bool push, uint8_t **out, int *out_len, bool full_keys) {
		char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
		uint8_t *p = (uint8_t *)&buf;

		cacheServer(server);

		if(out) {
			p = *out;
		}
		int len = 0;
		uint8_t flags = HAS_KEYS_FLAG;
		if(full_keys) {
			flags |= HAS_FULL_RULES_FLAG;
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
		

		std::vector<std::string>::iterator tok_it = m_last_list_req.field_list.begin();
		while (tok_it != m_last_list_req.field_list.end()) {
			if(usepopularlist) {
				BufferWriteByte(&p, &len, 0xFF);
			}
			if (server->kvFields.find((*tok_it)) != server->kvFields.end()) {
				std::string value = server->kvFields[*tok_it].c_str();
				BufferWriteNTS(&p, &len, (uint8_t*)value.c_str());
			}
			else {
				BufferWriteByte(&p, &len, 0);
			}
			tok_it++;
		}

		if(full_keys) {
			//std::map<int, std::map<std::string, std::string> > kvPlayers;
			std::map<int, std::map<std::string, std::string> >::iterator it = server->kvPlayers.begin();
			std::ostringstream s;
			std::pair<int, std::map<std::string, std::string> > index_mappair;
			std::map<std::string, std::string>::iterator it2;
			std::pair<std::string, std::string> kvp;

			

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
	void V2Peer::informNewServers(MM::ServerListQuery servers) {
		std::vector<MM::Server *>::iterator it = servers.list.begin();
		while(it != servers.list.end()) {
			MM::Server *s = *it;
			if(serverMatchesLastReq(s)) {
				if(FindServerByIP(s->wan_address).key[0] == 0) {
					sendServerData(s, false, true, NULL, NULL);
				}
			}
			it++;
		}
	}
	void V2Peer::informUpdateServers(MM::ServerListQuery servers) {
		std::vector<MM::Server *>::iterator it = servers.list.begin();
		while(it != servers.list.end()) {
			MM::Server *s = *it;
			if(serverMatchesLastReq(s)) {
				sServerCache cache = FindServerByIP(s->wan_address);
				if(cache.key[0] == 0 /*|| server matches new filter*/) {
					sendServerData(s, false, true, NULL, NULL, cache.full_keys);
				}
			}
			it++;
		}
	}
	sServerCache V2Peer::FindServerByIP(OS::Address address) {
		sServerCache ret;
		ret.full_keys = false;
		ret.key[0] = 0;
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				return cache;
			}
			it++;
		}
		return ret;		
	}
	void V2Peer::DeleteServerFromCacheByIP(OS::Address address) {
		std::vector<sServerCache>::iterator it = m_visible_servers.begin();
		while(it != m_visible_servers.end()) {
			sServerCache cache = *it;
			if(cache.wan_address.ip == address.ip && cache.wan_address.port == address.port) {
				it = m_visible_servers.erase(it);
				continue;
			}
			it++;
		}
	}
	void V2Peer::cacheServer(MM::Server *server) {
		sServerCache item;
		if(FindServerByIP(server->wan_address).key[0] == 0) {
			item.wan_address = server->wan_address;
			strcpy(item.key,server->key.c_str());
			item.full_keys = false;
			m_visible_servers.push_back(item);
		}
	}
	void V2Peer::ProcessInfoRequest(uint8_t *buffer, int remain) {
		uint8_t *p = (uint8_t *)buffer;
		int len = remain;
		OS::Address address;
		address.ip = Socket::htonl(BufferReadInt(&p, &len));
		address.port = Socket::htons(BufferReadShort(&p, &len));
		sServerCache cache = FindServerByIP(address);
		if(cache.key[0] != 0) {
			MM::Server *server = MM::GetServerByKey(cache.key);
			cache.full_keys = true;
			if(server) {
				sendServerData(server, false, true, NULL, NULL, true);
				delete server;
			}
		}
	}
}