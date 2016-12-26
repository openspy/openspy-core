#include <OS/OpenSpy.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/gsmsalg.h>
#include <OS/legacy/enctype_shared.h>
#include <OS/legacy/helpers.h>
#include <sstream>
#include "SBDriver.h"
#include "SBPeer.h"
#include "V1Peer.h"

#include <sstream>
#include <vector>
#include <algorithm>

#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>

namespace SB {
		V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : SB::Peer(driver, address_info, sd) {
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
			
			uint8_t buff[10];
			uint8_t *p = (uint8_t *)&buff;
			int len = 0;

			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > SB_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);
				//SendPacket((uint8_t *)&buff, len, true);
			}
		}
		void V1Peer::think(bool packet_waiting) {
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			socklen_t slen = sizeof(struct sockaddr_in);
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
			if(disconnect) {
				printf("Error: %s\n", fmt);
				m_delete_flag = true;
			}
		}
		void V1Peer::handle_gamename(char *data, int len) {
			char validation[16],realvalidate[16];
			char gamename[64];

			int enctype = find_paramint("enctype",(char *)data);
			find_param("gamename",(char *)data, (char *)&gamename,sizeof(gamename));
			m_game = OS::GetGameByName(gamename);

			printf("GOt game: %s\n",m_game.gamename);

			if(!m_game.gameid) {
				send_error(true, "Gamename not found");
				return;
			}

			if(enctype != 0) {
				find_param("validate",(char *)data,(char *)&validation,sizeof(validation));
				gsseckey((unsigned char *)&realvalidate, (unsigned char *)&m_challenge, (unsigned char *)m_game.secretkey, enctype);	
				if(strcmp(realvalidate,validation) == 0) {
					send_crypt_header(enctype);
					m_validated = true;
				} else {
					send_error(true, "Validation error");
					return;
				}
			}

			m_enctype = enctype;
		}
		void V1Peer::handle_packet(char *data, int len) {
			if(len < 0) {
				m_delete_flag = true;
				return;
			} else if(len == 0) {
				return;
			}

			printf("SB V1: %s\n",data);
			char command[32];
			gettimeofday(&m_last_recv, NULL);
			if(find_param(0, data,(char *)&command, sizeof(command))) {
			}
			if(strcmp(command,"gamename") == 0) {
				handle_gamename(data, len);
			} else if(strcmp(command, "list") == 0) {
				handle_list(data, len);
			}
		}
		void V1Peer::handle_list(char *data, int len) {
			char mode[32], gamename[32];
			
			if(!find_param("list",(char *)data, (char *)&mode,sizeof(mode)) || !find_param("gamename",(char *)data, (char *)&gamename,sizeof(gamename))) {
				send_error(true, "Data parsing error");
				return;
			}

			sServerListReq req;
			MM::ServerListQuery servers;
			req.m_for_game = OS::GetGameByName(gamename);
			req.m_from_game = m_game;

			if(req.m_from_game.gameid == 0) {
				send_error(false, "Gamename not found");
				return;
			}


			if(strcmp(mode,"cmp") == 0) {				
				servers = MM::GetServers(&req);
				SendServers(servers);
				FreeServerListQuery(&servers);
			} else if(strcmp(mode,"info2") == 0) {
				req.all_keys = true;
				servers = MM::GetServers(&req);
				SendServerInfo(servers);
				FreeServerListQuery(&servers);
			} else if(strcmp(mode,"groups") == 0) {
				req.all_keys = true;
				servers = MM::GetGroups(&req);
				SendGroups(servers);
				FreeServerListQuery(&servers);
			} else {
				send_error(true, "Unknown list mode");
				return;
			}

			m_delete_flag = true; //server disconnects after this
		}
		void V1Peer::SendServerInfo(MM::ServerListQuery results) {
			std::ostringstream resp;
			int field_count;
			struct sockaddr_in address_info;
			const char *ip_address;
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
				ip_address = Socket::inet_ntoa(address_info.sin_addr);
				resp << "\\" << ip_address << ":" << address_info.sin_port; //add ip/port
				while(it != results.captured_basic_fields.end()) {
					std::string field = *it;
					resp << "\\" << field_cleanup(serv->kvFields[field]);
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), true);
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
				for(int x=0;x<sizeof(cryptkey);x++) {
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
			for(int i=0;i<s.length();i++) {
				ch = s[i];
				if(ch == '\\') ch = '.';
				r += ch;
			}
			return r;
		}
}