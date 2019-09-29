#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include "Driver.h"
#include "Server.h"
#include "Peer.h"
namespace Peerchat {
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_sent_client_init = false;
		RegisterCommands();
	}
	Peer::~Peer() {
			
	}

	void Peer::OnConnectionReady() {
			
	}
	void Peer::Delete(bool timeout = false) {
			
	}
	
	void Peer::think(bool packet_waiting) {
		NetIOCommResp io_resp;
		if (m_delete_flag) return;

		if (packet_waiting) {
			OS::Buffer recv_buffer;
			io_resp = this->GetDriver()->getNetIOInterface()->streamRecv(m_sd, recv_buffer);

			int len = io_resp.comm_len;

			if (len <= 0) {
				goto end;
			}

			std::string command_upper;
			bool command_found = false;
			std::string recv_buf;
			recv_buf.append((const char *)recv_buffer.GetHead(), len);

			std::vector<std::string> commands = OS::KeyStringToVector(recv_buf, false, '\n');
			std::vector<std::string>::iterator it = commands.begin();
			while(it != commands.end()) {
				std::string command_line = OS::strip_whitespace(*it);
				std::vector<std::string> command_items = OS::KeyStringToVector(command_line, false, ' ');

				std::string command = command_items.at(0);

				command_upper = "";
				std::transform(command.begin(),command.end(),std::back_inserter(command_upper),toupper);
				
				std::vector<CommandEntry>::iterator it2 = m_commands.begin();
				while (it2 != m_commands.end()) {
					CommandEntry entry = *it2;
					if (command_upper.compare(entry.name) == 0) {
						if (entry.login_required) {
							if(m_user.id == 0) break;
						}
						command_found = true;
						printf("found command: %s\n", entry.name.c_str());
						(*this.*entry.callback)(command_items);
						break;
					}
					it2++;
				}					
				*it++;
			}
			if(!command_found) {
				std::ostringstream s;
				s << command_upper << " :Unknown Command";
				send_numeric(421, s.str(), true);
			}
		}

	end:
		//send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > PEERCHAT_PING_TIME * 2) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(OS::KVReader data_parser) {
			
	}

	int Peer::GetProfileID() {

	}

	void Peer::run_timed_operations() {
			
	}
	void Peer::SendPacket(std::string data) {
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)data.c_str(), data.length());

		//OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Send: %s", getAddress().ToString().c_str(), m_profile.id, data.c_str());

		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete();
		}
	}

	void Peer::RegisterCommands() {
		std::vector<CommandEntry> commands;
		commands.push_back(CommandEntry("NICK", false, &Peer::handle_nick));
		commands.push_back(CommandEntry("USER", false, &Peer::handle_user));
		commands.push_back(CommandEntry("PING", false, &Peer::handle_ping));
		commands.push_back(CommandEntry("OPER", false, &Peer::handle_oper));
		commands.push_back(CommandEntry("PRIVMSG", false, &Peer::handle_privmsg));
		commands.push_back(CommandEntry("JOIN", false, &Peer::handle_join));
		m_commands = commands;
	}
	void Peer::send_numeric(int num, std::string str, bool no_colon, std::string target_name) {
		std::ostringstream s;
		std::string name = "*";
		if(m_user_details.nick.size() > 0) {
			name = m_user_details.nick;
		}

		if(target_name.size()) {
			std::string nick = "*";
			if(m_user_details.nick.length()) {
				nick = m_user_details.nick;
			}
			name = nick + " " + target_name;
		}
		
		s << ":" << ((Peerchat::Server *)GetDriver()->getServer())->getServerName() << " " << std::setfill('0') << std::setw(3) << num << " " << name << " ";
		if(!no_colon) {
				s << ":";
		}
		s << str << std::endl;
		SendPacket(s.str());
	}
	void Peer::send_message(std::string messageType, std::string messageContent, std::string from, std::string to) {
		std::ostringstream s;

		if(from.length() == 0) {
			from = ((Peerchat::Server *)GetDriver()->getServer())->getServerName();
		}

		s << ":" << from;
		s << " " << messageType;
		if(to.length() > 0) {
			s << " " << to;
		}
		if(messageContent.length() > 0) {
			s << " :" << messageContent;
		}
		s << std::endl;
		SendPacket(s.str());
	}
	void Peer::OnUserRegistered(TaskResponse response_data, Peer *peer)  {
		std::ostringstream s;

		peer->m_user_details = response_data.summary;
		s << "Welcome to the Matrix " << peer->m_user_details.nick;
		peer->send_numeric(1, s.str());
		s.str("");

		s << "Your host is " << "SERVER NAME"  << ", running version 1.0";
		peer->send_numeric(2, s.str());
		s.str("");

		s << "This server was created Fri Oct 19 1979 at 21:50:00 PDT";
		peer->send_numeric(3, s.str());
		s.str("");

		s << "s 1.0 iq biklmnopqustvhe";
		peer->send_numeric(4, s.str(), true);
		s.str("");

		s << "- (M) Message of the day - ";
		peer->send_numeric(375, s.str());
		s.str("");

		s << "- Welcome to GameSpy";
		peer->send_numeric(372, s.str());
		s.str("");

		s << "End of MOTD command";
		peer->send_numeric(376, s.str());
		s.str("");

		s << "Your backend ID is " << response_data.summary.id;
		peer->send_numeric(377, s.str());
		s.str("");

	}
	void Peer::OnUserMaybeRegistered() {
		if(m_user_details.nick.length() == 0 || m_user_details.username.length() == 0 || m_sent_client_init) {
			return;
		}

		m_sent_client_init = true;

		TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetUserDetails;
		req.peer = this;
		req.summary = m_user_details;
		req.peer->IncRef();
		req.callback = OnUserRegistered;
		scheduler->AddRequest(req.type, req);
	}
	void Peer::OnRecvDirectMsg(std::string from, std::string msg, std::string type) {
		send_message(type, msg, from, m_user_details.nick);	
	}
}