#ifndef _PEERCHATPEER_H
#define _PEERCHATPEER_H
#include "../main.h"
#include <iomanip>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

#include <OS/SharedTasks/Auth/AuthTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>


#define PEERCHAT_PING_TIME 300

namespace Peerchat {
	class Driver;

	class TaskResponse;

	class Peer;
	typedef void(Peer::*CommandCallback)(std::vector<std::string>);
	class CommandEntry {
	public:
		CommandEntry(std::string name, bool login_required, int minimum_args, CommandCallback callback) {
			this->name = name;
			this->login_required = login_required;
			this->callback = callback;
			this->minimum_args = minimum_args;
		}
		std::string name;
		int login_required;
		bool minimum_args;
		CommandCallback callback;
	};

	class UserSummary {
		public:
			int id;
			std::string nick;
			std::string username;
			std::string hostname;
			std::string realname;
			OS::Address address;
			int gameid;
			std::string ToString() {
				return nick + "!" + username + "@" + address.ToString(true);
			};
	};



	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();

		void OnConnectionReady();
		void Delete(bool timeout = false);
		
		void think(bool packet_waiting);
		void handle_packet(OS::KVReader data_parser);

		int GetProfileID();

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void run_timed_operations();
		void SendPacket(std::string data);

		void RegisterCommands();

		void OnUserMaybeRegistered();
		UserSummary GetUserDetails() { return m_user_details; };
		void OnRecvDirectMsg(std::string from, std::string msg, std::string type);
		int GetBackendId() { return m_user_details.id; };

		void send_numeric(int num, std::string str, bool no_colon = false, std::string target_name = "", bool append_name = true);
		void send_message(std::string messageType, std::string messageContent, std::string from = "", std::string to = "", std::string target = "");

		int GetChannelFlags(int channel_id);

		void SendNickUpdate(std::string newNick);
	private:
		static void m_oper_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer);
		static void OnNickReserve(TaskResponse response_data, Peer *peer);
		static void OnUserRegistered(TaskResponse response_data, Peer *peer);
		static void OnNames_FetchChannelInfo(TaskResponse response_data, Peer *peer);
		static void OnMode_FetchChannelInfo(TaskResponse response_data, Peer* peer);
		static void OnTopic_FetchChannelInfo(TaskResponse response_data, Peer* peer);
		static void OnJoinChannel(TaskResponse response_data, Peer* peer);
		static void OnListChannels(TaskResponse response_data, Peer* peer);
		static void OnWhois_FetchUser(TaskResponse response_data, Peer* peer);
		static void OnSetCKey(TaskResponse response_data, Peer* peer);
		static void OnGetCKey(TaskResponse response_data, Peer* peer);
		static void OnSetKey(TaskResponse response_data, Peer* peer);
		static void OnGetKey(TaskResponse response_data, Peer* peer);
		static void OnSetChanKey(TaskResponse response_data, Peer* peer);
		static void OnGetChanKey(TaskResponse response_data, Peer* peer);
		static void OnKickCallback(TaskResponse response_data, Peer* peer);

		void handle_nick(std::vector<std::string> data_parser);
		void handle_user(std::vector<std::string> data_parser);
		void handle_ping(std::vector<std::string> data_parser);		
		void handle_oper(std::vector<std::string> data_parser);		
		void handle_privmsg(std::vector<std::string> data_parser);
		void handle_notice(std::vector<std::string> data_parser);
		void handle_utm(std::vector<std::string> data_parser);
		void handle_atm(std::vector<std::string> data_parser);
		void handle_join(std::vector<std::string> data_parser);
		void handle_part(std::vector<std::string> data_parser);
		void handle_kick(std::vector<std::string> data_parser);
		void handle_topic(std::vector<std::string> data_parser);
		void handle_names(std::vector<std::string> data_parser);
		void handle_mode(std::vector<std::string> data_parser);
		void handle_userhost(std::vector<std::string> data_parser);
		void handle_list(std::vector<std::string> data_parser);
		void handle_whois(std::vector<std::string> data_parser);
		void handle_quit(std::vector<std::string> data_parser);
		void handle_setckey(std::vector<std::string> data_parser);
		void handle_getckey(std::vector<std::string> data_parser);
		void handle_setkey(std::vector<std::string> data_parser);
		void handle_getkey(std::vector<std::string> data_parser);
		void handle_setchankey(std::vector<std::string> data_parser);
		void handle_getchankey(std::vector<std::string> data_parser);

		void send_no_such_target_error(std::string channel);
		void send_topic(std::string channel);

		void handle_message_command(std::string type, std::vector<std::string> data_parser);


		OS::GameData m_game;
		
		struct timeval m_last_recv, m_last_ping;

		OS::User m_user;
		OS::Profile m_profile;

		UserSummary m_user_details;

		OS::CMutex *mp_mutex;

		std::vector<CommandEntry> m_commands;

		bool m_sent_client_init;

		std::map<int, int> m_channel_flags;
	};
}
#endif //_GPPEER_H