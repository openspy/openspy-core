#include <OS/OpenSpy.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetServer.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/legacy/gsmsalg.h>
#include <OS/legacy/enctype_shared.h>
#include <OS/legacy/helpers.h>


#include <sstream>
#include "ChatDriver.h"
#include "ChatServer.h"
#include "ChatPeer.h"
#include "IRCPeer.h"
#include "ChatBackend.h"


#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>


namespace Chat {
		IRCCommandHandler IRCPeer::mp_command_handler[] = {
			//user cmds
			{"NICK", &IRCPeer::handle_nick},
			{"USER", &IRCPeer::handle_user},
			{"PING", &IRCPeer::handle_ping},
			{"PONG", &IRCPeer::handle_pong},
			{"WHOIS", &IRCPeer::handle_whois},
			{"USERHOST", &IRCPeer::handle_userhost},
			{"PRIVMSG", &IRCPeer::handle_privmsg},
			{"NOTICE", &IRCPeer::handle_privmsg},
			{"UTM", &IRCPeer::handle_privmsg},
			{"ATM", &IRCPeer::handle_privmsg},
			
			{"SETKEY", &IRCPeer::handle_setkey},
			{"GETKEY", &IRCPeer::handle_getkey},

			{"KICK", &IRCPeer::handle_kick},
			{"OPER", &IRCPeer::handle_oper},


			//channel cmds
			{"JOIN", &IRCPeer::handle_join},
			{"QUIT", &IRCPeer::handle_quit},
			{"PART", &IRCPeer::handle_part},
			{"NAMES", &IRCPeer::handle_names},

			{"MODE", &IRCPeer::handle_mode},
			{"TOPIC", &IRCPeer::handle_topic},

			{"SETCKEY", &IRCPeer::handle_setckey},
			{"GETCKEY", &IRCPeer::handle_getckey},

			{"SETCHANKEY", &IRCPeer::handle_setchankey},
			{"GETCHANKEY", &IRCPeer::handle_getchankey},

			{"LISTUSERMODES", &IRCPeer::handle_listusermodes},	
			{"SETUSERMODE", &IRCPeer::handle_setusermode},
			/*{"LISTCHANPROPS", &IRCPeer::handle_listchanprops},
			
			{"DELCHANPROPS", &IRCPeer::handle_listchanprops},
			{"SETCHANPROPS", &IRCPeer::handle_listchanprops},

			{"LISTUSERMODES", &IRCPeer::handle_listchanprops},
			{"DELUSERMODE", &IRCPeer::handle_listchanprops},
			
			*/
		};
		IRCPeer::IRCPeer(Driver *driver, struct sockaddr_in *address_info, int sd) : Chat::Peer(driver, address_info, sd) {
			m_sent_client_init = false;

			gettimeofday(&m_last_ping, NULL);

			m_client_info.ip = OS::Address(*address_info);
			m_client_info.hostname = m_client_info.ip.ToString(true);

			m_default_partnercode = 0;
		}
		IRCPeer::~IRCPeer() {
			std::ostringstream s;
			s << "ERROR : Closing Link: " << ((ChatServer*)mp_driver->getServer())->getName() << " (" << m_quit_reason << ")" << std::endl;
			SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
			ChatBackendTask::SubmitClientDelete(NULL, this, mp_driver, m_quit_reason);
		}
		
		void IRCPeer::send_ping() {
			//check for timeout
			struct timeval current_time;

			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > CHAT_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);


				std::ostringstream s;
				s << "PING " << ((ChatServer*)mp_driver->getServer())->getName() << " :" << time(NULL) << std::endl;
				SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
			}


		}
		void IRCPeer::think(bool packet_waiting) {
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			int len;
			if (packet_waiting) {
				len = recv(m_sd, (char *)&buf, MAX_OUTGOING_REQUEST_SIZE, 0);
				buf[len] = 0;
				if(len == 0) goto end;
				std::vector<std::string> cmds =  OS::split(buf, '\n');
				std::string cmd_line;

				std::vector<std::string>::iterator it = cmds.begin();
				while(it != cmds.end()) {
					std::string cmd_line = *it;
					it++;
					std::vector<std::string> x = OS::split(cmd_line, ' ');

					std::string cmd = x.front();

					EIRCCommandHandlerRet ret = EIRCCommandHandlerRet_Unknown;

					//printf("IRC got cmd(%s): %s\n",cmd.c_str(), cmd_line.c_str());

					for(unsigned int i=0;i<sizeof(mp_command_handler)/sizeof(IRCCommandHandler);i++) {
						if(strcasecmp(mp_command_handler[i].command.c_str(), cmd.c_str()) == 0) {
							ret = (*this.*(mp_command_handler[i].mpFunc))(x, cmd_line);
						}
					}
					send_command_error(ret, cmd);

				}
				gettimeofday(&m_last_recv, NULL);
				
			}

			end:
			send_ping();

			//check for timeout
			struct timeval current_time;
			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_recv.tv_sec > CHAT_PING_TIME*2) {
				m_delete_flag = true;
				m_timeout_flag = true;
			} else if(len == 0 && packet_waiting) {
				m_delete_flag = true;
			}
		}
		void IRCPeer::send_command_error(EIRCCommandHandlerRet value, std::string cmd_name) {
			std::ostringstream s;
			switch(value) {
				case EIRCCommandHandlerRet_Unknown:
					s << cmd_name << " :Unknown Command";
					send_numeric(421, s.str(), true);
				break;
				case EIRCCommandHandlerRet_NotEnoughParams:
					s << cmd_name << " :Not enough parameters";
					send_numeric(461, s.str(), true);
				break;
				default:
				break;
			}
		}
		void IRCPeer::send_nonick_channel_error(std::string name) {
			std::ostringstream s;
			s << name << " :No such nick/channel";
			send_numeric(401, s.str(), true);
		}
		void IRCPeer::OnRecvClientMessage(ChatClientInfo from_user, const char *msg, EChatMessageType message_type) {
			std::ostringstream s;
			const char *type_privmsg = "PRIVMSG";
			const char *type_notice = "NOTICE";
			const char *type_utm = "UTM";
			const char *type_atm = "ATM";
			const char *type;
			switch(message_type) {
				case EChatMessageType_Notice:
					type = type_notice;
				break;
				case EChatMessageType_UTM:
					type = type_utm;
				break;
				case EChatMessageType_ATM:
					type = type_atm;
				break;
				default:
				case EChatMessageType_Msg:
					type = type_privmsg;
				break;
			}
			s << ":" << from_user.name << "!" << from_user.user << "@" << from_user.hostname << " " << type << " " << m_client_info.name << " :" << msg << std::endl;
			SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
		}
		void IRCPeer::OnRecvChannelMessage(ChatClientInfo from_user, ChatChannelInfo to_channel, const char *msg, EChatMessageType message_type) {
			if(from_user.client_id == m_client_info.client_id) {
				return;
			}
			std::ostringstream s;
			const char *type_privmsg = "PRIVMSG";
			const char *type_notice = "NOTICE";
			const char *type_utm = "UTM";
			const char *type_atm = "ATM";
			const char *type;
			switch(message_type) {
				case EChatMessageType_Notice:
					type = type_notice;
				break;
				case EChatMessageType_UTM:
					type = type_utm;
				break;
				case EChatMessageType_ATM:
					type = type_atm;
				break;
				default:
				case EChatMessageType_Msg:
					type = type_privmsg;
				break;
			}
			s << ":" << from_user.name << "!" << from_user.user << "@" << from_user.hostname << " " << type << " " << to_channel.name << " :" << msg << std::endl;
			SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
		}
		void IRCPeer::send_numeric(int num, std::string str, bool no_colon, std::string target_name) {
			std::ostringstream s;
			std::string name = "*";
			if(m_client_info.name.size() > 0) {
				name = m_client_info.name;
			}

			if(target_name.size()) {
				name = target_name;
			}
			
			s << ":" << ((ChatServer*)mp_driver->getServer())->getName() << " " << std::setfill('0') << std::setw(3) << num << " " << name << " ";
			if(!no_colon) {
				 s << ":";
			}
			s << str << std::endl;
			SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
		}


		void IRCPeer::SendPacket(const uint8_t *buff, int len) {
			uint8_t out_buff[MAX_OUTGOING_REQUEST_SIZE * 2];
			uint8_t *p = (uint8_t*)&out_buff;
			int out_len = 0;
			BufferWriteData(&p, &out_len, buff, len);
			int c = send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL);
			if(c < 0) {
				m_delete_flag = true;
			}
		}
		void IRCPeer::send_client_init() {
			if(m_sent_client_init) {
				return;
			}
			m_sent_client_init = true;
			std::ostringstream s;

			s << "Welcome to the Matrix " << m_client_info.name;
			send_numeric(1, s.str());
			s.str("");

			s << "Your host is " << ((ChatServer*)mp_driver->getServer())->getName()  << ", running version 1.0";
			send_numeric(2, s.str());
			s.str("");

			s << "This server was created Fri Oct 19 1979 at 21:50:00 PDT";
			send_numeric(3, s.str());
			s.str("");

			s << "s 1.0 iq biklmnopqustvhe";
			send_numeric(4, s.str(), true);
			s.str("");

			s << "- (M) Message of the day - ";
			send_numeric(375, s.str());
			s.str("");

			s << "- Welcome to GameSpy";
			send_numeric(372, s.str());
			s.str("");

			s << "End of MOTD command";
			send_numeric(376, s.str());
			s.str("");
		}
		bool IRCPeer::is_channel_name(std::string name) {
			return name[0] == '#';
		}
		bool IRCPeer::send_callback_error(const struct Chat::_ChatQueryRequest request, const struct Chat::_ChatQueryResponse response) {
			std::ostringstream s;
			std::string name;
			std::pair<EChatBackendResponseError, std::string> p;
			std::vector< std::pair<EChatBackendResponseError, std::string> >::const_iterator it = response.errors.begin();
			bool ret = true;
			while(it != response.errors.end()) {
				p = *it;
				switch(p.first) {

					case EChatBackendResponseError_NoUser_OrChan:
					if(!request.query_name.length()) {
						name = request.query_data.client_info.name;
					} else {
						name = request.query_name;
					}
					s << name << " :No such nick/channel";
					send_numeric(401, s.str(), true);
					break;
					case EChatBackendResponseError_NoVoicePerms:
					s << response.channel_info.name << " :You're not voiced";
					send_numeric(482, s.str(), true);
					break;
					case EChatBackendResponseError_NoHOPPerms:
					s << response.channel_info.name << ":You're not channel half operator";
					send_numeric(482, s.str(), true);
					break;
					case EChatBackendResponseError_NoOPPerms:
					s << response.channel_info.name << ":You're not channel operator";
					send_numeric(482, s.str(), true);
					break;
					case EChatBackendResponseError_NoOwnerPerms:
					s << response.channel_info.name << ":You're not channel owner";
					send_numeric(482, s.str(), true);
					break;
					case EChatBackendResponseError_NickInUse:
					s << response.channel_info.name << ":Nickname is already in use";
					send_numeric(433, s.str(), true);
					break;
					case EChatBackendResponseError_BadPermissions:
						s << response.channel_info.name << " :" << p.second;
						send_numeric(482, s.str(), true);
					break;
					case EChatBackendResponseError_NoChange:
					default:
					ret = false;
					break;
				}
				it++;
			}
			return ret && response.errors.size() > 0;
		}
		void IRCPeer::OnUserQuit(ChatClientInfo client, std::string quit_reason) {
			std::ostringstream s;
			if(m_client_channel_hits.find(client.client_id) != m_client_channel_hits.end()) {
				if(m_client_channel_hits[client.client_id].m_hits > 0 || m_client_channel_hits[client.client_id].m_op_hits > 0) {
					s << ":" << client.name << "!" << client.user << "@" << client.hostname << " QUIT :" << quit_reason  << std::endl;
					SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
					m_client_channel_hits.erase(client.client_id);
				}
			}
		}
}