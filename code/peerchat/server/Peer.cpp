#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>


#include "Driver.h"
#include "Server.h"
#include "Peer.h"
namespace Peerchat {
	const CommandEntry Peer::m_commands[] = {
		CommandEntry("CRYPT", false, 3, &Peer::handle_crypt),
		CommandEntry("NICK", false, 1 ,&Peer::handle_nick, 0, 150),
		CommandEntry("REGISTERNICK", false, 1 ,&Peer::handle_registernick, 0, 150),		 //register unique nick
		CommandEntry("USER", false, 4, &Peer::handle_user),
		CommandEntry("PING", false, 0, &Peer::handle_ping),
		CommandEntry("PONG", false, 0, &Peer::handle_pong),
		CommandEntry("CDKEY", false, 1, &Peer::handle_cdkey),
		CommandEntry("OPER", false, 3, &Peer::handle_oper,0, 250),
		CommandEntry("LOGIN", false, 3, &Peer::handle_login, 0, 250),
		CommandEntry("LOGINPREAUTH", false, 2, &Peer::handle_loginpreauth, 0, 250),
		CommandEntry("PRIVMSG", true, 2, &Peer::handle_privmsg),
		CommandEntry("NOTICE", true, 2, &Peer::handle_notice),
		CommandEntry("UTM", true, 2, &Peer::handle_utm),
		CommandEntry("ATM", true, 2, &Peer::handle_atm),
		CommandEntry("JOIN", true, 1, &Peer::handle_join, 0, 100),
		CommandEntry("PART", true, 1, &Peer::handle_part),
		CommandEntry("QUIT", false, 1, &Peer::handle_quit),
		CommandEntry("MODE", true, 1, &Peer::handle_mode, 0, 100),
		CommandEntry("NAMES", true, 1, &Peer::handle_names),
		CommandEntry("USRIP", false, 0, &Peer::handle_userhost),
		CommandEntry("USERHOST", false, 0, &Peer::handle_userhost),
		CommandEntry("TOPIC", true, 1, &Peer::handle_topic, 0, 100),
		CommandEntry("LIST", true, 0, &Peer::handle_list, 0, 300),
		CommandEntry("LISTLIMIT", true, 2, &Peer::handle_listlimit, 0, 300),
		CommandEntry("WHOIS", true, 1, &Peer::handle_whois, 0, 100),
		CommandEntry("WHO", true, 1, &Peer::handle_who),
		CommandEntry("SETCKEY", true, 3, &Peer::handle_setckey, 0, 10),
		CommandEntry("GETCKEY", true, 5, &Peer::handle_getckey, 0, 10),
		CommandEntry("SETCHANKEY", true, 2, &Peer::handle_setchankey, 0, 10),
		CommandEntry("GETCHANKEY", true, 4, &Peer::handle_getchankey, 0, 10),
		CommandEntry("SETKEY", true, 4, &Peer::handle_setkey, 0, 10),
		CommandEntry("GETKEY", true, 4, &Peer::handle_getkey, 0, 10),
		CommandEntry("KICK", true, 2, &Peer::handle_kick, 0, 200),
		CommandEntry("SETGROUP", true, 2, &Peer::handle_setgroup, 0, 100),
		CommandEntry("LUSERS", true, 0, &Peer::handle_lusers, OPERPRIVS_LISTOPERS),
		CommandEntry("KILL", true, 2, &Peer::handle_kill, OPERPRIVS_KILL),
		CommandEntry("SETUSERMODE", true, 2, &Peer::handle_setusermode, OPERPRIVS_GLOBALOWNER),
		CommandEntry("DELUSERMODE", true, 1, &Peer::handle_delusermode, OPERPRIVS_GLOBALOWNER),
		CommandEntry("LISTUSERMODES", true, 1, &Peer::handle_listusermodes, OPERPRIVS_GLOBALOWNER),
		CommandEntry("LISTCHANPROPS", true, 1, &Peer::handle_listchanprops, OPERPRIVS_GLOBALOWNER),
		CommandEntry("SETCHANPROPS", true, 1, &Peer::handle_setchanprops, OPERPRIVS_GLOBALOWNER),
		CommandEntry("DELCHANPROPS", true, 1, &Peer::handle_delchanprops, OPERPRIVS_GLOBALOWNER),
	};
	Peer::Peer(Driver *driver, uv_tcp_t *sd) : INetPeer(driver, sd) {
		m_user_details.id = 0;
		mp_user_address_visibility_list = new OS::LinkedListHead<UserAddressVisibiltyInfo*>();
		m_sent_client_init = false;
		m_stream_waiting = false;
		uv_mutex_init(&m_mutex);
		m_using_encryption = false;
		m_got_delete = false;
		m_flood_weight = 0;
		gettimeofday(&m_last_recv, NULL);
		gettimeofday(&m_last_sent_ping, NULL);
		gettimeofday(&m_connect_time, NULL);		
		gettimeofday(&m_last_keepalive, NULL);

		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetUserDetails;
		req.peer = this;
		req.peer->IncRef();
		req.callback = OnGetBackendId;
		AddPeerchatTaskRequest(req);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", getAddress().ToString().c_str(), m_timeout_flag);

		PurgeUserAddressVisibility();
		delete mp_user_address_visibility_list;
		
		uv_mutex_destroy(&m_mutex);
		
	}
	void Peer::OnGetBackendId(TaskResponse response_data, Peer *peer) {
		peer->m_user_details.id = response_data.summary.id;
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
		if(m_delete_flag || m_got_delete) {
			return;
		}
		m_got_delete = true; //mark as "deleted", so that async delete related tasks can be completed, which will later set the true delete flag after async events complete
		m_timeout_flag = timeout;

		send_quit(reason);
	}
	
	void Peer::on_stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
		OS::Buffer recv_buffer;
		recv_buffer.WriteBuffer(buf->base, nread);

		gettimeofday(&m_last_recv, NULL);

		if(m_using_encryption) {
			gs_crypt((unsigned char*)recv_buffer.GetHead(), nread, &m_crypt_key_in);
		}

		

		std::string command_upper;
		bool command_found = false;
		std::string recv_buf;
		recv_buf.append((const char *)recv_buffer.GetHead(), nread);

		OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Recv: %s", getAddress().ToString().c_str(), m_profile.id, recv_buf.c_str());

		std::vector<std::string> commands = OS::KeyStringToVector(recv_buf, false, '\n');
		std::vector<std::string>::iterator it = commands.begin();
		while(it != commands.end() && !m_delete_flag) {
			std::string command_line = OS::strip_whitespace(*it);
			std::vector<std::string> command_items = OS::KeyStringToVector(command_line, false, ' ');

			if (command_items.size() == 0) break;
			std::string command = command_items.at(0);

			command_upper = "";
			std::transform(command.begin(),command.end(),std::back_inserter(command_upper),toupper);
			
			for(size_t i=0;i<sizeof(m_commands)/sizeof(CommandEntry);i++) {
				CommandEntry entry = m_commands[i];
				if (command_upper.compare(entry.name) == 0) {
					if (entry.login_required) {
						if(m_user_details.id == 0 || !m_sent_client_init) break;
					}
					if(entry.required_operflags != 0) {
						if(!(GetOperFlags() & entry.required_operflags)) {
							break;
						}
					}
					command_found = true;

					if (((int)command_items.size()) >= entry.minimum_args+1) {
						m_flood_weight += entry.weight;
						if(m_flood_weight >= WARNING_FLOOD_WEIGHT_THRESHOLD) {
							send_flood_warning();
						}
						(*this.*entry.callback)(command_items);
					}
					else {
						send_numeric(461, command_upper + " :Not enough parameters", true);
					}
					break;
				}
			}					
			*it++;
		}
		if(!command_found) {
			std::ostringstream s;
			s << command_upper << " :Unknown Command";
			send_numeric(421, s.str(), true);
		}
	}
	void Peer::think(bool packet_waiting) {
		if (m_delete_flag) return;

		if(m_stream_waiting) {
			packet_waiting = true;
			m_stream_waiting = false;
		}

		if(m_user_details.id == 0 && packet_waiting) {
			m_stream_waiting = true;
			packet_waiting = false; //don't process anything until you have a backend id
		}

	end:
		send_ping();

		perform_keepalive();

		if(!(GetOperFlags() & OPERPRIVS_FLOODEXCEMPT)) {
			if(m_flood_weight >= MAX_FLOOD_WEIGHT) {
				Delete(false, "Excess flood");
				return;
			}
		}

		if(m_flood_weight > 0) {
			m_flood_weight -= FLOOD_DECR_PER_TICK;
		}
		if(m_flood_weight < 0)
			m_flood_weight = 0;
		

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_connect_time.tv_sec > REGISTRATION_TIMEOUT && !m_sent_client_init) {
			Delete(true, "Registration Timeout");
		}
		else if (current_time.tv_sec - m_last_recv.tv_sec > PEERCHAT_PING_TIMEOUT_TIME) {
			Delete(true, "Ping Timeout");
		}
	}
	void Peer::send_ping() {
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_sent_ping.tv_sec > PEERCHAT_PING_INTERVAL) {
				gettimeofday(&m_last_sent_ping, NULL);
				char ping_key[9];

				memset(&ping_key, 0, sizeof(ping_key));
				OS::gen_random((char *)&ping_key, sizeof(ping_key)-1);
				
				std::ostringstream s;
				s << " PING " << " :" << ping_key;
				s << std::endl;
				SendPacket(s.str());
		}
	}
	void Peer::perform_keepalive() {
		struct timeval current_time;
		gettimeofday(&current_time, NULL);


		if (current_time.tv_sec - m_last_keepalive.tv_sec > PEERCHAT_PING_TIMEOUT_TIME) {
			gettimeofday(&m_last_keepalive, NULL);
			PeerchatBackendRequest req;
			req.type = EPeerchatRequestType_KeepaliveUser;
			req.summary = GetUserDetails();
			req.peer = this;
			req.peer->IncRef();
			req.callback = NULL;
			AddPeerchatTaskRequest(req);
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

		OS::LogText(OS::ELogLevel_Debug, "[%s] (%d) Send: %s", getAddress().ToString().c_str(), m_profile.id, data.c_str());

		if(m_using_encryption) {
			gs_crypt((unsigned char *)buffer.GetHead(), buffer.bytesWritten(), &m_crypt_key_out);
		}

		append_send_buffer(buffer);
	}

	void Peer::send_numeric(int num, std::string str, bool no_colon, std::string target_name, bool append_name, std::string default_name) {
		std::ostringstream s;
		std::string name = default_name;
		if(m_user_details.nick.size() > 0) {
			name = m_user_details.nick;
		}

		if(target_name.size()) {
			std::string nick = default_name;
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
	void Peer::send_message(std::string messageType, std::string messageContent, UserSummary from, std::string to, std::string target, bool no_colon) {
		if (m_user_details.modeflags & EUserMode_Quiet) {
			return;
		}
		std::ostringstream s;


		std::string from_string;

		if(from.id == 0) {
			from_string = ((Peerchat::Server *)GetDriver()->getServer())->getServerName();
		} else {
			from_string = from.ToIRCString(IsUserAddressVisible(from.id));
		}

		s << ":" << from_string;
		s << " " << messageType;
		if(to.length() > 0) {
			s << " " << to;
		}
		if(target.length() > 0) {
			s << " " << target;
		}
		if(messageContent.length() > 0) {
			s << " ";
			if(!no_colon) {
				s << ":";
			}
			s << messageContent;
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

		s << "Your host is " << ((Peerchat::Driver *)peer->GetDriver())->getServerName()  << ", running version 1.0";
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

		s << "Your backend ID is " << peer->GetBackendId();
		peer->send_numeric(377, s.str());
		s.str("");
	}
	void Peer::OnLookupGlobalUsermode(TaskResponse response_data, Peer *peer)  {
		if(response_data.usermode.modeflags & EUserChannelFlag_Banned) {
			peer->Delete(false, "KILLED - KLINE");
			return;
		}
		if(response_data.usermode.modeflags & EUserChannelFlag_Gagged) {
			peer->m_user_details.modeflags |= EUserMode_Gagged;
		}
		peer->refresh_user_details(true);
	}
	void Peer::refresh_user_details(bool user_registration) {
		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_SetUserDetails;
		req.peer = this;
		req.summary = GetUserDetails();
		req.peer->IncRef();
		req.callback = user_registration ? OnUserRegistered : NULL;
		AddPeerchatTaskRequest(req);
	}
	void Peer::OnUserMaybeRegistered() {
		if(m_user_details.nick.length() == 0 || m_user_details.username.length() == 0 || m_sent_client_init) {
			return;
		}

		m_sent_client_init = true;

		PeerchatBackendRequest req;
		req.type = EPeerchatRequestType_LookupGlobalUsermode;
		req.peer = this;
		req.summary = m_user_details;
		req.peer->IncRef();
		req.callback = OnLookupGlobalUsermode;
		AddPeerchatTaskRequest(req);
	}
	void Peer::OnRecvDirectMsg(UserSummary from, std::string msg, std::string type) {
		send_message(type, msg, from.ToString(), m_user_details.nick);	
	}
	void Peer::send_no_such_target_error(std::string channel) {
		send_numeric(401, "No such nick/channel", false, channel);
	}
	int Peer::GetChannelFlags(int channel_id) {
		int flags = 0;
		uv_mutex_lock(&m_mutex);
		if (m_channel_flags.find(channel_id) != m_channel_flags.end()) {
			flags = m_channel_flags[channel_id];
		}		
		uv_mutex_unlock(&m_mutex);
		return flags;
	}
	void Peer::SetChannelFlags(int channel_id, int mode_flags) {
		
		int current_modeflags = 0;
		if(m_channel_flags.find(channel_id) != m_channel_flags.end()) {
			current_modeflags = m_channel_flags[channel_id];
		}

		uv_mutex_lock(&m_mutex);
		m_channel_flags[channel_id] = mode_flags;
		uv_mutex_unlock(&m_mutex);

		
		//check if needs to refresh user address visibility list for channel or purge channel address visibility
		int current_modelevel = GetUserChannelModeLevel(current_modeflags);
		int modelevel = GetUserChannelModeLevel(mode_flags);
		if(current_modelevel <= 1 && modelevel > 1) {
			RefreshUserAddressVisibility_ByChannel(channel_id);
		} else if(modelevel <= 1) {
			RemoveUserAddressVisibility_ByChannel(channel_id);
		}

	}
	std::vector<int> Peer::GetChannels() {
		std::vector<int> channels;
		uv_mutex_lock(&m_mutex);
		std::map<int, int>::iterator it = m_channel_flags.begin();
		while (it != m_channel_flags.end()) {
			std::pair<int, int> p = *it;
			channels.push_back(p.first);
			it++;
		}
		uv_mutex_unlock(&m_mutex);
		return channels;
	}
	void Peer::send_flood_warning() {
		std::ostringstream ss;
		ss << "Excess Flood: " << m_flood_weight;
		send_message("PRIVMSG", ss.str(), *server_userSummary, m_user_details.nick);
	}
	void Peer::OnRemoteDisconnect(std::string reason) {
		Delete(false, reason);
	}
	bool Peer::LLIterator_IsUserAddressVisible(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state) {
		if(info->user_id == state->user_id) {
			state->found = true;
			return false;
		}
		return true;
	}
	bool Peer::IsUserAddressVisible(int user_id) {
		if(GetOperFlags() & OPERPRIVS_GLOBALOWNER) {
			return true;
		}
		IterateUserAddressVisibiltyInfoState state;
		state.user_id = user_id;
		state.peer = this;
		OS::LinkedListIterator<UserAddressVisibiltyInfo*, IterateUserAddressVisibiltyInfoState*> iterator(mp_user_address_visibility_list);
		iterator.Iterate(LLIterator_IsUserAddressVisible, &state);
		return state.found;
	}
	void Peer::AddUserToAddressVisbilityList(int user_id, int channel_id) {
		uv_mutex_lock(&m_mutex);
		mp_user_address_visibility_list->AddToList(new UserAddressVisibiltyInfo(user_id, channel_id));
		uv_mutex_unlock(&m_mutex);
	}
	bool Peer::LLIterator_RemoveUserAddressVisibility_ByChannel(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state) {
		if(info->channel_id == state->channel_id) {
			state->peer->mp_user_address_visibility_list->RemoveFromList(info);
			delete info;
		}
		return true;
	}
	void Peer::RemoveUserAddressVisibility_ByChannel(int channel_id) {
		IterateUserAddressVisibiltyInfoState state;
		state.channel_id = channel_id;
		state.peer = this;
		OS::LinkedListIterator<UserAddressVisibiltyInfo*, IterateUserAddressVisibiltyInfoState*> iterator(mp_user_address_visibility_list);
		uv_mutex_lock(&m_mutex);
		iterator.Iterate(LLIterator_RemoveUserAddressVisibility_ByChannel, &state);
		uv_mutex_unlock(&m_mutex);
	}
	bool Peer::LLIterator_RemoveUserAddressVisibility_ByChannelUser(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state) {
		if(info->channel_id == state->channel_id && info->user_id == state->user_id) {
			state->peer->mp_user_address_visibility_list->RemoveFromList(info);
			delete info;
		}
		return true;
	}
	void Peer::RemoveUserAddressVisibility_ByChannelUser(int user_id, int channel_id) {
		IterateUserAddressVisibiltyInfoState state;
		state.channel_id = channel_id;
		state.user_id = user_id;
		state.peer = this;
		OS::LinkedListIterator<UserAddressVisibiltyInfo*, IterateUserAddressVisibiltyInfoState*> iterator(mp_user_address_visibility_list);
		uv_mutex_lock(&m_mutex);
		iterator.Iterate(LLIterator_RemoveUserAddressVisibility_ByChannelUser, &state);
		uv_mutex_unlock(&m_mutex);
	}
	bool Peer::LLIterator_RemoveUserAddressVisibility_ByUser(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state) {
		if(info->user_id == state->user_id) {
			state->peer->mp_user_address_visibility_list->RemoveFromList(info);
			delete info;
		}
		return true;
	}
	void Peer::RemoveUserAddressVisibility_ByUser(int user_id) {
		IterateUserAddressVisibiltyInfoState state;
		state.user_id = user_id;
		state.peer = this;
		OS::LinkedListIterator<UserAddressVisibiltyInfo*, IterateUserAddressVisibiltyInfoState*> iterator(mp_user_address_visibility_list);
		uv_mutex_lock(&m_mutex);
		iterator.Iterate(LLIterator_RemoveUserAddressVisibility_ByUser, &state);
		uv_mutex_unlock(&m_mutex);
	}
	void Peer::OnLookup_RefreshUserAddressVisibility_ByChannel(TaskResponse response_data, Peer *peer) {
		std::vector<ChannelUserSummary>::iterator it = response_data.channel_summary.users.begin();
		while (it != response_data.channel_summary.users.end()) {
			ChannelUserSummary user = *(it++);
			if(user.userSummary.id != peer->GetBackendId()) {
				peer->AddUserToAddressVisbilityList(user.userSummary.id, response_data.channel_summary.channel_id);
			}
		}
	}
	void Peer::RefreshUserAddressVisibility_ByChannel(int channel_id) {
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_LookupChannelDetails;
        req.peer = this;
        req.channel_summary.channel_id = channel_id;
        req.peer->IncRef();
        req.callback = OnLookup_RefreshUserAddressVisibility_ByChannel;
        AddPeerchatTaskRequest(req);
	}
	bool Peer::LLIterator_PurgeUserAddressVisibility(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state) {
		state->peer->mp_user_address_visibility_list->RemoveFromList(info);
		delete info;
		return true;
	}
	void Peer::PurgeUserAddressVisibility() {
		IterateUserAddressVisibiltyInfoState state;
		state.peer = this;
		OS::LinkedListIterator<UserAddressVisibiltyInfo*, IterateUserAddressVisibiltyInfoState*> iterator(mp_user_address_visibility_list);
		uv_mutex_lock(&m_mutex);
		iterator.Iterate(LLIterator_RemoveUserAddressVisibility_ByUser, &state);
		uv_mutex_unlock(&m_mutex);
	}
	void Peer::OnSetExternalUserChanModeFlags(int user_id, int channel_id, int modeflags) {
		int current_modeflags = GetChannelFlags(channel_id);
		//int current_modelevel = GetUserChannelModeLevel(current_modeflags);
		int modelevel = GetUserChannelModeLevel(modeflags);
		bool has_op = GetUserChannelModeLevel(current_modeflags) > 1;
		if((modeflags & EUserChannelFlag_IsInChannel) && (current_modeflags & EUserChannelFlag_IsInChannel) && has_op && current_modeflags >= modelevel) {
			AddUserToAddressVisbilityList(user_id, channel_id);
		} else if(!(modeflags & EUserChannelFlag_IsInChannel)) {
			RemoveUserAddressVisibility_ByChannelUser(user_id, channel_id);
		}
	}
}
