#include <OS/OpenSpy.h>
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

#include <OS/Buffer.h>

#include <OS/legacy/helpers.h> //gen_random
#include <OS/KVReader.h>

#include <OS/Net/NetServer.h>

namespace SB {
		V1Peer::V1Peer(Driver *driver, INetIOSocket *sd) : SB::Peer(driver, sd, 1) {
			std::ostringstream s;

			memset(&m_challenge,0,sizeof(m_challenge));
			gen_random((char *)&m_challenge,6);

			m_enctype = 0;
			m_waiting_gamedata = 0;
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
			NetIOCommResp io_resp;
			if (m_delete_flag) return;
			if (m_waiting_gamedata == 2) {
				m_waiting_gamedata = 0;
				while (!m_waiting_packets.empty()) {
					std::string cmd = m_waiting_packets.front();
					m_waiting_packets.pop();
					handle_packet(cmd.c_str(), cmd.length());
				}
			}
			if (packet_waiting) {
				io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, m_recv_buffer);

				int len = io_resp.comm_len;

				if ((io_resp.disconnect_flag || io_resp.error_flag) && len <= 0) {
					goto end;
				}

				std::string recv_buf((const char *)m_recv_buffer.GetHead(), len);

				m_peer_stats.packets_in++;
				m_peer_stats.bytes_in += len;

				/* split by \\final\\  */
				char *p = (char *)recv_buf.c_str();
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
				Delete(true);
			} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
				Delete();
			}
		}

		void V1Peer::informDeleteServers(MM::Server *server) {

		}
		void V1Peer::informNewServers(MM::Server *server) {

		}
		void V1Peer::informUpdateServers(MM::Server *server) {

		}
		void V1Peer::send_error(bool disconnect, const char *fmt, ...) {
			char str[512];
			std::ostringstream resp;
			va_list args;
			va_start(args, fmt);
			int len = vsnprintf(str, sizeof(str), fmt, args);
			str[len] = 0;
			va_end(args);
			if(disconnect) {
				resp << "\\fatal\\1";
			}
			resp << "\\error\\" << str;

			OS::LogText(OS::ELogLevel_Info, "[%s] Got Error %s", m_sd->address.ToString().c_str(), str);
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), true);

			if (disconnect) {
				Delete();
			}
		}
		void V1Peer::handle_gamename(const char *data, int len) {
			OS::KVReader data_parser = OS::KVReader(std::string(data));
			MM::MMQueryRequest req;

			std::string validation;

			int enctype = data_parser.GetValueInt("enctype");
			std::string gamename = data_parser.GetValue("gamename");

		
			validation = data_parser.GetValue("validate");
			m_validation = validation;

			req.type = MM::EMMQueryRequestType_GetGameInfoByGameName;
			m_waiting_gamedata = 1;
			req.gamenames[0] = gamename;
			req.extra = (void *)2;
			m_enctype = enctype;
			AddRequest(req);
		}
		void V1Peer::handle_packet(const char *data, int len) {
			if(len < 0) {
				Delete();
				return;
			} else if(len == 0) {
				return;
			}
			if (m_waiting_gamedata == 1) {
				m_waiting_packets.push(std::string(data));
				return;
			}
			OS::KVReader kv_parser = OS::KVReader(std::string(data));

			if (kv_parser.Size() < 1) {
				Delete();
				return;
			}

			std::string command;
			gettimeofday(&m_last_recv, NULL);

			command = kv_parser.GetKeyByIdx(0);
			if(strcmp(command.c_str(),"gamename") == 0 && !m_validated) {
				handle_gamename(data, len);
			}
			else if (strcmp(command.c_str(), "list") == 0) {
				handle_list(data, len);
			}
			else if (strcmp(command.c_str(), "queryid") == 0) {

			}
			else {
				//send_error(true, "Cannot handle request");
				OS::LogText(OS::ELogLevel_Info, "[%s] Got Unknown request %s", m_sd->address.ToString().c_str(), data);
			}
		}
		void V1Peer::OnRetrievedServerInfo(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			SendServerInfo(results);
		}
		void V1Peer::OnRetrievedServers(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			if (request.req.all_keys) {
				SendServerInfo(results);
			}
			else {
				SendServers(results);
			}			
		}
		void V1Peer::OnRetrievedGroups(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra) {
			SendGroups(results);
		}
		void V1Peer::OnRecievedGameInfo(const OS::GameData game_data, void *extra) {
			int type = (int)extra;
			m_waiting_gamedata = 2;
			if (type == 1) {
				if (game_data.secretkey[0] == 0) {
					send_error(true, "Invalid target gamename");
					return;
				}
				MM::MMQueryRequest req;
				m_last_list_req.m_from_game = m_game;
				m_last_list_req.m_for_game = game_data;

				req.req = m_last_list_req;
				req.type = req.req.send_groups ? MM::EMMQueryRequestType_GetGroups : MM::EMMQueryRequestType_GetServers;
				AddRequest(req);
			}
			else if (type == 2) {
				char realvalidate[16];
				if (game_data.secretkey[0] == 0) {
					send_error(true, "Invalid source gamename");
					return;
				}
				m_game = game_data;
				m_peer_stats.from_game = m_game;
				gsseckey((unsigned char *)&realvalidate, (unsigned char *)&m_challenge, (const unsigned char *)m_game.secretkey.c_str(), m_enctype);
				if(strcmp(realvalidate,m_validation.c_str()) == 0) {
					send_crypt_header(m_enctype);
					m_validated = true;
				} else {
					send_error(true, "Validation error");
					return;
				}
			}
			FlushPendingRequests();
		}
		void V1Peer::handle_list(const char *data, int len) {
			std::string mode, gamename;

			OS::KVReader kv_parser(data);

			if (m_game.secretkey[0] == 0) {
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
			m_waiting_gamedata = 1;
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

			OS::LogText(OS::ELogLevel_Info, "[%s] List Request: gamenames: (%s) - (%s), fields: %s  is_group: %d, all_keys: %d", m_sd->address.ToString().c_str(), req.req.m_from_game.gamename.c_str(), req.req.m_for_gamename.c_str(), req.req.filter.c_str(), req.req.send_groups, req.req.all_keys);

			req.extra = (void *)1;
			m_last_list_req = req.req;
			AddRequest(req);
			// //server disconnects after this
		}
		void V1Peer::SendServerInfo(MM::ServerListQuery results) {
			std::ostringstream resp;
			int field_count;

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

				resp << "\\" << serv->wan_address.ToString(); //add ip/port
				while(it != results.captured_basic_fields.end()) {
					std::string field = *it;
					resp << "\\" << field_cleanup(serv->kvFields[field]);
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)resp.str().c_str(), resp.str().length(), results.last_set);

			//if(results.last_set)
				//m_delete_flag = true;
		}
		void V1Peer::SendGroups(MM::ServerListQuery results) {
			OS::Buffer buffer;

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

			buffer.WriteBuffer((void *)resp.str().c_str(), resp.str().length());

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
						buffer.WriteByte('\\');
						while(it3 != serv->kvFields.end()) {
							std::pair<std::string, std::string> kv_pair = *it3;
							if(std::find(group_fields.begin(), group_fields.end(), kv_pair.first) == group_fields.end()) {
								buffer.WriteByte(0x01);
								buffer.WriteBuffer((void *)field_cleanup(kv_pair.first).c_str(), kv_pair.first.length());
								buffer.WriteByte(0x01);
								buffer.WriteBuffer((void*)field_cleanup(kv_pair.second).c_str(), kv_pair.second.length());
							}
							it3++;
						}
						buffer.WriteByte('\\');
					} else {
						buffer.WriteByte('\\');
						buffer.WriteBuffer((void*)field_cleanup(serv->kvFields[*it]).c_str(), serv->kvFields[*it].length());
					}
					it++;
				}
				it2++;
			}
			SendPacket((const uint8_t *)buffer.GetHead(), buffer.size(), results.last_set);
		}
		void V1Peer::SendServers(MM::ServerListQuery results) {
			OS::Buffer buffer;


			//std::vector<Server *> list;
			std::vector<MM::Server *>::iterator it = results.list.begin();
			while(it != results.list.end()) {
				MM::Server *serv = *it;
				buffer.WriteInt(serv->wan_address.GetIP());
				buffer.WriteShort(serv->wan_address.GetPort());
				it++;
			}
			SendPacket((const uint8_t *)buffer.GetHead(), buffer.size(), results.last_set);
			if (results.last_set) {
				Delete();
			}
		}
		void V1Peer::SendPacket(const uint8_t *buff, int len, bool attach_final, bool skip_encryption) {
			OS::Buffer buffer;
			buffer.WriteBuffer((void *)buff, len);
			if(attach_final) {
				buffer.WriteBuffer("\\final\\", 7);
			}
			if (!skip_encryption) {
				switch (m_enctype) {
				case 2:
					m_keyptr = encshare1((unsigned int *)&m_cryptkey_enctype2, (unsigned char *)buffer.GetHead(), buffer.size(), m_keyptr);
					break;
				}
			}

			m_peer_stats.bytes_out += buffer.size();
			m_peer_stats.packets_out++;
			NetIOCommResp io_resp;
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, buffer);
			if(io_resp.disconnect_flag || io_resp.error_flag) {
				Delete();
			}
		}
		void V1Peer::send_crypt_header(int enctype) {
			OS::Buffer buffer;
			char buff[256];
			char *p = (char *)(&buff);
			int len = 0;
			char cryptkey[13];
			int secretkeylen = m_game.secretkey.length();
			if(enctype == 2) {
				for(unsigned int x=0;x<sizeof(cryptkey);x++) {
					cryptkey[x] = (uint8_t)rand();
				}
				encshare4((unsigned char *)&cryptkey, sizeof(cryptkey),(unsigned int *)&m_cryptkey_enctype2);
				for(int i=0;i< secretkeylen;i++) {
					cryptkey[i] ^= m_game.secretkey[i];
				}
				buffer.WriteByte(sizeof(cryptkey) ^ 0xEC);
				buffer.WriteBuffer((uint8_t *)&cryptkey, sizeof(cryptkey));
				SendPacket((const uint8_t *)&buff, len, false, true);
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