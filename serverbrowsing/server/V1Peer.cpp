#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/gsmsalg.h>
#include <OS/legacy/enctype_shared.h>
#include <sstream>
#include "SBDriver.h"
#include "SBPeer.h"
#include "V1Peer.h"

#include <sstream>
#include <vector>
#include <algorithm>

#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>


#include <OS/legacy/helpers.h> //gen_random
#include <OS/KVReader.h>

namespace SB {
		V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : SB::Peer(driver, address_info, sd, 1) {
			std::ostringstream s;

			memset(&m_challenge,0,sizeof(m_challenge));
			gen_random((char *)&m_challenge,6);

			m_enctype = 0;
			m_validated = false;
			m_keyptr = NULL;

			s << "\\basic\\\\secure\\" << m_challenge;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length(), true);
		}
		V1Peer::~V1Peer() {

		}
		
		void V1Peer::send_ping() {
			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > SB_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);
				//SendPacket((uint8_t *)&buff, len, true);
			}
		}
		void V1Peer::think(bool packet_waiting) {
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			int len;
			if (packet_waiting) {
				len = recv(m_sd, (char *)&buf, MAX_OUTGOING_REQUEST_SIZE, 0);
				buf[len] = 0;
				if(len == 0) goto end;

				/* split by \\final\\  */
				char *p = (char *)&buf;
				char *x;
				while(true) {
					x = p;
					p = strstr(p,"\\final\\");
					if(p == NULL) { break; }
					*p = 0;
					this->handle_packet(x, strlen(x));
					p+=7;
				}
				this->handle_packet(x, strlen(x));

				
			}

			end:
			send_ping();

			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_recv.tv_sec > SB_PING_TIME*2) {
				m_delete_flag = true;
				m_timeout_flag = true;
			} else if(len == 0 && packet_waiting) {
				m_delete_flag = true;
			}
		}

		void V1Peer::informDeleteServers(MM::Server *server) {

		}
		void V1Peer::informNewServers(MM::Server *server) {

		}
		void V1Peer::informUpdateServers(MM::Server *server) {

		}
		void V1Peer::send_error(bool disconnect, const char *fmt, ...) {
			char str[256];
			std::ostringstream resp;
			va_list args;
			va_start(args, fmt);
			vsprintf(str, fmt, args);
			va_end(args);
			if(disconnect) {
				resp << "\\fatal\\1";
				m_delete_flag = true;
			}
			resp << "\\error\\" << str;

			OS::LogText(OS::ELogLevel_Info, "[%s] Got Error %s", OS::Address(m_address_info).ToString().c_str(), resp.str().c_str());
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), true);
		}
		void V1Peer::handle_gamename(char *data, int len) {
			OS::KVReader data_parser = OS::KVReader(std::string(data));
			MM::MMQueryRequest req;

			std::string validation;

			int enctype = data_parser.GetValueInt("enctype");
			std::string gamename = data_parser.GetValue("gamename");

			if(!m_game.gameid) {
				send_error(true, "Gamename not found");
				return;
			}

			if(enctype != 0) {
				validation = data_parser.GetValue("validate");
				m_validation = validation;
			}
			req.type = MM::EMMQueryRequestType_GetGameInfoByGameName;
			req.gamenames[0] = gamename;
			req.extra = (void *)2;
			req.driver = mp_driver;
			req.peer = this;
			req.peer->IncRef();
			m_enctype = enctype;
			MM::m_task_pool->AddRequest(req);
		}
		void V1Peer::handle_packet(char *data, int len) {
			if(len < 0) {
				m_delete_flag = true;
				return;
			} else if(len == 0) {
				return;
			}
			OS::KVReader kv_parser = OS::KVReader(std::string(data));

			std::string command;
			gettimeofday(&m_last_recv, NULL);

			command = kv_parser.GetValueByIdx(0);
			if(strcmp(command.c_str(),"gamename") == 0 && !m_validated) {
				handle_gamename(data, len);
			} else if(strcmp(command.c_str(), "list") == 0 && m_validated) {
				handle_list(data, len);
			}
			else {
				send_error(true, "Cannot handle request");
			}
		}
		void V1Peer::OnRetrievedServerInfo(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			SendServerInfo(results);
		}
		void V1Peer::OnRetrievedServers(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			SendServers(results);
		}
		void V1Peer::OnRetrievedGroups(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			SendGroups(results);
		}
		void V1Peer::OnRecievedGameInfo(const OS::GameData game_data, void *extra) {
			int type = (int)extra;
			if (type == 1) {
				if (game_data.gameid == 0) {
					send_error(true, "Invalid target gamename");
					return;
				}
				MM::MMQueryRequest req;
				m_last_list_req.m_from_game = m_game;
				m_last_list_req.m_for_game = game_data;
				req.req = m_last_list_req;
				req.type = req.req.send_groups ? MM::EMMQueryRequestType_GetGroups : MM::EMMQueryRequestType_GetServers;
				req.driver = mp_driver;
				req.peer = this;
				req.peer->IncRef();
				MM::m_task_pool->AddRequest(req);
			}
			else if (type == 2) {
				char realvalidate[16];
				if (game_data.gameid == 0) {
					send_error(true, "Invalid source gamename");
					return;
				}
				m_game = game_data;
				gsseckey((unsigned char *)&realvalidate, (unsigned char *)&m_challenge, (unsigned char *)m_game.secretkey, m_enctype);
				if(strcmp(realvalidate,m_validation.c_str()) == 0) {
					send_crypt_header(m_enctype);
					m_validated = true;
				} else {
					send_error(true, "Validation error");
					return;
				}
			}
		}
		void V1Peer::handle_list(char *data, int len) {
			std::string mode, gamename;

			OS::KVReader kv_parser(data);

			if (m_game.gameid == 0) {
				send_error(true, "No source gamename registered");
				return;
			}

			if(!kv_parser.HasKey("list") || !kv_parser.HasKey("gamename")) {
				send_error(true, "Data parsing error");
				return;
			} else {
				mode = kv_parser.GetValue("list");
				gamename = kv_parser.GetValue("gamename");
			}

			MM::MMQueryRequest req;
			MM::ServerListQuery servers;
			req.gamenames[0] = gamename;
			req.req.m_for_gamename = gamename;
			req.req.m_from_game = m_game;
			req.req.all_keys = false;


			req.type = MM::EMMQueryRequestType_GetGameInfoByGameName;
			req.req.send_groups = false;
			if(mode.compare("cmp") == 0) {				
				req.req.all_keys = false;
			} else if(mode.compare("info2") == 0) {
				req.req.all_keys = true;
			} else if(mode.compare("groups") == 0) {
				req.req.send_groups = true;
				req.req.all_keys = true;
			} else {
				send_error(true, "Unknown list mode");
				return;
			}

			OS::LogText(OS::ELogLevel_Info, "[%s] List Request: gamenames: (%s) - (%s), fields: %s  is_group: %d, all_keys: %d", OS::Address(m_address_info).ToString().c_str(), req.req.m_from_gamename.c_str(), req.req.m_for_gamename.c_str(), req.req.send_groups, req.req.all_keys);

			req.extra = (void *)1;
			m_last_list_req = req.req;
			req.driver = mp_driver;
			req.peer = this;
			req.peer->IncRef();
			MM::m_task_pool->AddRequest(req);
			// //server disconnects after this
		}
		void V1Peer::SendServerInfo(MM::ServerListQuery results) {
			std::ostringstream resp;
			int field_count;
			struct sockaddr_in address_info;

			char ip_address[ADDR_STR_LEN];

			std::vector<std::string>::iterator it;
			results.captured_basic_fields.insert(results.captured_basic_fields.begin(), "ip"); //insert ip/port field
			field_count = results.captured_basic_fields.size(); // + results.captured_player_fields.size() + results.captured_team_fields.size();
			it = results.captured_basic_fields.begin();
			resp << "\\fieldcount\\" << field_count;
			while(it != results.captured_basic_fields.end()) {
				resp << "\\" << *it;
				it++;
			}

			std::vector<MM::Server *>::iterator it2 = results.list.begin();
			while(it2 != results.list.end()) {
				MM::Server *serv = *it2;

				it = results.captured_basic_fields.begin();
				address_info.sin_port = serv->wan_address.port;
				address_info.sin_addr.s_addr = Socket::htonl(serv->wan_address.ip);
				Socket::inet_ntop(AF_INET, &(address_info.sin_addr), ip_address, ADDR_STR_LEN);

				resp << "\\" << ip_address << ":" << address_info.sin_port; //add ip/port
				while(it != results.captured_basic_fields.end()) {
					std::string field = *it;
					resp << "\\" << field_cleanup(serv->kvFields[field]);
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), true);
			m_delete_flag = true;
		}
		void V1Peer::SendGroups(MM::ServerListQuery results) {
			uint8_t buff[MAX_OUTGOING_REQUEST_SIZE * 2];
			uint8_t *p = (uint8_t *)&buff;
			int len = 0;

			std::ostringstream resp;
			std::vector<std::string> group_fields;
			int fieldcount;

			group_fields.push_back("groupid");
			group_fields.push_back("hostname");
			group_fields.push_back("numplayers");
			group_fields.push_back("maxwaiting");
			group_fields.push_back("numwaiting");
			group_fields.push_back("password");
			group_fields.push_back("other");
						
			fieldcount = group_fields.size();

			resp << "\\fieldcount\\" << fieldcount;
			std::vector<std::string>::iterator it = group_fields.begin();
			while(it != group_fields.end()) {
				resp << "\\" << *it;
				it++;
			}

			BufferWriteData(&p, &len, (const uint8_t *)resp.str().c_str(), resp.str().length());

			std::vector<MM::Server *>::iterator it2 = results.list.begin();
			while(it2 != results.list.end()) {
				MM::Server *serv = *it2;
				std::vector<std::string>::iterator it = group_fields.begin();
				while(it != group_fields.end()) {
					if((*it).compare("other") == 0) {
						//print everything that is not in group_fields from kvFields
						std::map<std::string, std::string>::iterator it3 = serv->kvFields.begin();
						std::ostringstream field_data_ss;
						std::string field_data;
						BufferWriteByte(&p, &len, '\\');
						while(it3 != serv->kvFields.end()) {
							std::pair<std::string, std::string> kv_pair = *it3;
							if(std::find(group_fields.begin(), group_fields.end(), kv_pair.first) == group_fields.end()) {
								BufferWriteByte(&p, &len, 0x01);
								BufferWriteData(&p, &len, (const uint8_t*)field_cleanup(kv_pair.first).c_str(), kv_pair.first.length());
								BufferWriteByte(&p, &len, 0x01);
								BufferWriteData(&p, &len, (const uint8_t*)field_cleanup(kv_pair.second).c_str(), kv_pair.second.length());
							}
							it3++;
						}
						BufferWriteByte(&p, &len, '\\');
					} else {
						BufferWriteByte(&p, &len, '\\');
						BufferWriteData(&p, &len, (const uint8_t *)field_cleanup(serv->kvFields[*it]).c_str(), serv->kvFields[*it].length());
					}
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)buff, len, true);
		}
		void V1Peer::SendServers(MM::ServerListQuery results) {
			uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE + 1];
			uint8_t *p = (uint8_t*)&out_buff;
			int len = 0;


			//std::vector<Server *> list;
			std::vector<MM::Server *>::iterator it = results.list.begin();
			while(it != results.list.end()) {
				MM::Server *serv = *it;
				BufferWriteInt((uint8_t **)&p,&len,Socket::htonl(serv->wan_address.ip));
				BufferWriteShort((uint8_t **)&p,&len,Socket::htons(serv->wan_address.port));
				it++;
			}
			SendPacket((const uint8_t *)&out_buff, len, true);
			m_delete_flag = true;
		}
		void V1Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
			uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
			uint8_t *p = (uint8_t*)&out_buff;
			int out_len = 0;
			BufferWriteData(&p, &out_len, buff, len);
			if(attach_final) {
				BufferWriteData(&p, &out_len, (uint8_t*)"\\final\\", 7);
			}
			switch(m_enctype) {
				case 2:
					m_keyptr = encshare1((unsigned int *)&m_cryptkey_enctype2, (unsigned char *)&out_buff, out_len,m_keyptr);
				break;
			}
			int c = send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL);
			if(c < 0) {
				m_delete_flag = true;
			}
		}
		void V1Peer::send_crypt_header(int enctype) {
			char buff[256];
			char *p = (char *)(&buff);
			int len = 0;
			char cryptkey[13];
			int secretkeylen = strlen(m_game.secretkey);
			if(enctype == 2) {
				for(unsigned int x=0;x<sizeof(cryptkey);x++) {
					cryptkey[x] = (uint8_t)rand();
				}
				encshare4((unsigned char *)&cryptkey, sizeof(cryptkey),(unsigned int *)&m_cryptkey_enctype2);
				for(int i=0;i< secretkeylen;i++) {
					cryptkey[i] ^= m_game.secretkey[i];
				}
				BufferWriteByte((uint8_t **)&p,&len,sizeof(cryptkey)^0xEC);
				BufferWriteData((uint8_t **)&p, &len, (uint8_t *)&cryptkey, sizeof(cryptkey));
				SendPacket((const uint8_t *)&buff, len, false);
			}

		}
		std::string V1Peer::field_cleanup(std::string s) {
			std::string r;
			char ch;
			for(unsigned int i=0;i<s.length();i++) {
				ch = s[i];
				if(ch == '\\') ch = '.';
				r += ch;
			}
			return r;
		}


		void V1Peer::OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra) {

		}
}