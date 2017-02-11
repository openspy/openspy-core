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

#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>

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


			//channel cmds
			{"JOIN", &IRCPeer::handle_join},
			//{"PART", &IRCPeer::handle_part},
		};
		IRCPeer::IRCPeer(Driver *driver, struct sockaddr_in *address_info, int sd) : Chat::Peer(driver, address_info, sd) {
			m_sent_client_init = false;
			m_fresh_client_info = false;

			gettimeofday(&m_last_ping, NULL);

			m_client_info.ip = OS::Address(*address_info);
			m_client_info.hostname = m_client_info.ip.ToString(true);

			m_fresh_client_info = false;
		}
		IRCPeer::~IRCPeer() {
			printf("Peerchat delete\n");
		}
		
		void IRCPeer::send_ping() {
			//check for timeout
			struct timeval current_time;

			gettimeofday(&current_time, NULL);
			if(current_time.tv_sec - m_last_ping.tv_sec > CHAT_PING_TIME) {
				gettimeofday(&m_last_ping, NULL);


				std::ostringstream s;
				s << "PING :" << ((ChatServer*)mp_driver->getServer())->getName() << std::endl;
				SendPacket((const uint8_t*)s.str().c_str(),s.str().length());
			}

			if(m_fresh_client_info) {
				ChatBackendTask::SubmitClientInfo(OnNickCmd_SubmitClientInfo, this, mp_driver); //submit in main thread
				m_fresh_client_info = false;
			}


		}
		void IRCPeer::think(bool packet_waiting) {
			char buf[MAX_OUTGOING_REQUEST_SIZE + 1];
			socklen_t slen = sizeof(struct sockaddr_in);
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

					for(int i=0;i<sizeof(mp_command_handler)/sizeof(IRCCommandHandler);i++) {
						if(strcmp(mp_command_handler[i].command.c_str(), cmd.c_str()) == 0) {
							ret = (*this.*(mp_command_handler[i].mpFunc))(x, cmd_line);
						}
					}
					send_command_error(ret, cmd);

				}
				gettimeofday(&m_last_recv, NULL);
				
			}

			end:
			send_ping();

			//queue stuff
			if(m_fresh_client_info) {
				ChatBackendTask::SubmitClientInfo(OnNickCmd_SubmitClientInfo, this, mp_driver); //submit in main thread
				m_fresh_client_info = false;
			}

			handle_queues();
			//

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
				default:
				break;
			}
		}
		void IRCPeer::send_nonick_channel_error(std::string name) {
			std::ostringstream s;
			s << name << " :No such nick/channel";
			send_numeric(401, s.str(), true);
		}
		void IRCPeer::OnRecvClientMessage(ChatClientInfo from_user, const char *msg) {
			std::ostringstream s;
			s << ":" << from_user.name << "!" << from_user.user << "@" << from_user.hostname << " PRIVMSG " << m_client_info.name << " :" << msg << std::endl;
			SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
		}
		void IRCPeer::send_numeric(int num, std::string str, bool no_colon) {
			std::ostringstream s;
			std::string name = "*";
			if(m_client_info.name.size() > 0) {
				name = m_client_info.name;
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
}