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
		m_user_details.id = 0;
		m_user_details.modeflags = 0;
		m_oper_flags = 0;
		mp_mutex = OS::CreateMutex();
		m_using_encryption = false;
		RegisterCommands();
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}

	void Peer::OnConnectionReady() {
		m_user_details.address = getAddress();
		m_user_details.hostname = m_user_details.address.ToString(true);

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", getAddress().ToString().c_str());
	}
	void Peer::Delete(bool timeout) {
		Delete(false, "Client Exited");
	}
	void Peer::Delete(bool timeout, std::string reason) {
		send_quit(reason);
		m_delete_flag = true;
		m_timeout_flag = timeout;

		if (m_user_details.id != 0) {
			TaskScheduler<PeerchatBackendRequest, TaskThreadData>* scheduler = ((Peerchat::Server*)(GetDriver()->getServer()))->GetPeerchatTask();
			PeerchatBackendRequest req;
			req.type = EPeerchatRequestType_DeleteUser;
			req.peer = this;
			req.peer->IncRef();
			req.callback = NULL;
			scheduler->AddRequest(req.type, req);
		}
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

			if(m_using_encryption) {
				gs_crypt(recv_buffer.GetHead(), len, &m_crypt_key_in);
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
							if(m_user_details.id == 0) break;
						}
						command_found = true;

						if (((int)command_items.size()) >= entry.minimum_args+1) {
							(*this.*entry.callback)(command_items);
						}
						else {
							send_numeric(461, command_upper + " :Not enough parameters", true);
						}
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
			Delete(true, "Ping Timeout");
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete(false, "Connection severed");
		}
	}
	void Peer::handle_packet(OS::KVReader data_parser) {
			
	}

	int Peer::GetProfileID() {
		return m_profile.id;
	}

	void Peer::run_timed_operations() {
			
	}
	void Peer::SendPacket(std::string data) {
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)data.c_str(), data.length());

		if(m_using_encryption) {
			gs_crypt(buffer.GetHead(), buffer.bytesWritten(), &m_crypt_key_out);
		}

		//OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Send: %s", getAddress().ToString().c_str(), m_profile.id, data.c_str());

		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getNetIOInterface()->streamSend(m_sd, buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete(false, "Connection severed");
		}
	}

	void Peer::RegisterCommands() {
		std::vector<CommandEntry> commands;
		commands.push_back(CommandEntry("CRYPT", false, 3, &Peer::handle_crypt));
		commands.push_back(CommandEntry("NICK", false, 1 ,&Peer::handle_nick));
		commands.push_back(CommandEntry("USER", false, 4, &Peer::handle_user));
		commands.push_back(CommandEntry("PING", false, 0, &Peer::handle_ping));
		commands.push_back(CommandEntry("OPER", false, 3, &Peer::handle_oper));
		commands.push_back(CommandEntry("PRIVMSG", true, 2, &Peer::handle_privmsg));
		commands.push_back(CommandEntry("NOTICE", true, 2, &Peer::handle_notice));
		commands.push_back(CommandEntry("UTM", true, 2, &Peer::handle_utm));
		commands.push_back(CommandEntry("ATM", true, 2, &Peer::handle_atm));
		commands.push_back(CommandEntry("JOIN", true, 1, &Peer::handle_join));
		commands.push_back(CommandEntry("PART", true, 1, &Peer::handle_part));
		commands.push_back(CommandEntry("QUIT", false, 1, &Peer::handle_quit));
		commands.push_back(CommandEntry("MODE", true, 1, &Peer::handle_mode));
		commands.push_back(CommandEntry("NAMES", true, 1, &Peer::handle_names));
		commands.push_back(CommandEntry("USRIP", false, 0, &Peer::handle_userhost));
		commands.push_back(CommandEntry("USERHOST", false, 0, &Peer::handle_userhost));
		commands.push_back(CommandEntry("TOPIC", true, 1, &Peer::handle_topic));
		commands.push_back(CommandEntry("LIST", true, 1, &Peer::handle_list));
		commands.push_back(CommandEntry("LISTLIMIT", true, 2, &Peer::handle_listlimit));
		commands.push_back(CommandEntry("WHOIS", true, 1, &Peer::handle_whois));
		commands.push_back(CommandEntry("WHO", true, 1, &Peer::handle_who));
		commands.push_back(CommandEntry("SETCKEY", true, 5, &Peer::handle_setckey));
		commands.push_back(CommandEntry("GETCKEY", true, 5, &Peer::handle_getckey));
		commands.push_back(CommandEntry("SETCHANKEY", true, 4, &Peer::handle_setchankey));
		commands.push_back(CommandEntry("GETCHANKEY", true, 4, &Peer::handle_getchankey));
		commands.push_back(CommandEntry("SETKEY", true, 4, &Peer::handle_setkey));
		commands.push_back(CommandEntry("GETKEY", true, 4, &Peer::handle_getkey));
		commands.push_back(CommandEntry("KICK", true, 2, &Peer::handle_kick));
		commands.push_back(CommandEntry("SETGROUP", true, 2, &Peer::handle_setgroup));


		commands.push_back(CommandEntry("SETUSERMODE", true, 2, &Peer::handle_setusermode));
		commands.push_back(CommandEntry("DELUSERMODE", true, 1, &Peer::handle_delusermode));
		commands.push_back(CommandEntry("LISTUSERMODES", true, 1, &Peer::handle_listusermodes));
		m_commands = commands;
	}
	void Peer::send_numeric(int num, std::string str, bool no_colon, std::string target_name, bool append_name) {
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
			if (append_name) {
				name = nick + " " + target_name;
			}
			else {
				name = target_name;
			}
			
		}
		
		s << ":" << ((Peerchat::Server *)GetDriver()->getServer())->getServerName() << " " << std::setfill('0') << std::setw(3) << num << " " << name << " ";
		if(!no_colon) {
			s << ":";
		}
		s << str << std::endl;
		SendPacket(s.str());
	}
	void Peer::send_message(std::string messageType, std::string messageContent, std::string from, std::string to, std::string target) {
		if (m_user_details.modeflags & EUserMode_Quiet) {
			return;
		}
		std::ostringstream s;

		if(from.length() == 0) {
			from = ((Peerchat::Server *)GetDriver()->getServer())->getServerName();
		}

		s << ":" << from;
		s << " " << messageType;
		if(to.length() > 0) {
			s << " " << to;
		}
		if(target.length() > 0) {
			s << " " << target;
		}
		if(messageContent.length() > 0) {
			s << " :" << messageContent;
		}
		s << std::endl;
		SendPacket(s.str());

		if (messageType.compare("JOIN") == 0 && from.compare(GetUserDetails().ToString()) == 0) {
			//send names automatically
			std::vector<std::string> params;
			params.push_back("NAMES");
			params.push_back(to);
			handle_names(params);
		}
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
	void Peer::OnRecvDirectMsg(UserSummary from, std::string msg, std::string type) {
		send_message(type, msg, from.ToString(), m_user_details.nick);	
	}
	void Peer::send_no_such_target_error(std::string channel) {
		send_numeric(401, "No such nick/channel", false, channel);
	}
	int Peer::GetChannelFlags(int channel_id) {
		int flags = 0;
		mp_mutex->lock();
		if (m_channel_flags.find(channel_id) != m_channel_flags.end()) {
			flags = m_channel_flags[channel_id];
		}		
		mp_mutex->unlock();
		return flags;
	}
	std::vector<int> Peer::GetChannels() {
		std::vector<int> channels;
		mp_mutex->lock();
		std::map<int, int>::iterator it = m_channel_flags.begin();
		while (it != m_channel_flags.end()) {
			std::pair<int, int> p = *it;
			channels.push_back(p.first);
			it++;
		}
		mp_mutex->unlock();
		return channels;
	}
}