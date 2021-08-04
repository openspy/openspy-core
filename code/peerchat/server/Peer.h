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

#include <sstream>
#include <algorithm>

#include "chatCrypt.h"
#include <server/irc_common.h>

#define PEERCHAT_PING_TIME 300
#define MAX_FLOOD_WEIGHT 1600
#define FLOOD_DECR_PER_TICK 25
#define WARNING_FLOOD_WEIGHT_THRESHOLD 1200
#define REGISTRATION_TIMEOUT 60
#define MAX_USER_CHANNELS 32


namespace Peerchat {
	class Driver;

	class TaskResponse;
	class ChannelSummary;
	class UsermodeRecord;
	class ChanpropsRecord;

	class Peer;
	typedef void(Peer::*CommandCallback)(std::vector<std::string>);
	class CommandEntry {
	public:
		CommandEntry(std::string name, bool login_required, int minimum_args, CommandCallback callback, int required_operflags = 0, int weight = 50) {
			this->name = name;
			this->login_required = login_required;
			this->callback = callback;
			this->minimum_args = minimum_args;
			this->required_operflags = required_operflags;
			this->weight = weight;
		}
		std::string name;
		bool login_required;
		int minimum_args;
		int required_operflags;
		int weight;
		CommandCallback callback;
	};

	class UserSummary {
		
		public:
			UserSummary() {
				id = 0;	
				operflags = 0;
				profileid = 0;
				userid = 0;
				modeflags = 0;
				gameid = -1;
			}
			
			UserSummary(std::string string) {
				size_t username_marker = string.find_first_of("!");
				size_t address_marker = string.find_first_of("@");

				size_t id_marker = string.find_first_of("+");

				nick = string.substr(0, username_marker);
				username = string.substr(username_marker + 1, address_marker - username_marker - 1);

				size_t address_end = std::string::npos;
				
				if(id_marker != -1) {
					std::string id_data = string.substr(id_marker + 1);
					id = atoi(id_data.c_str());
					address_end = id_marker - 1;
				}
				address = string.substr(address_marker + 1, id_marker - address_marker - 1);
				hostname = address.ToString(true);
			}
			int id;
			std::string nick;
			std::string username;
			std::string hostname;
			std::string realname;
			OS::Address address;
			int gameid;
			int profileid;
			int userid;
			int modeflags;
			int operflags;
			std::string ToIRCString(bool address_visible = false) {
				std::ostringstream ss;
				ss << nick << "!";
				ss << username << "@";
				ss << GetIRCAddress(address_visible);
				return ss.str();				
			}
			std::string GetIRCAddress(bool address_visible = false) {
				if(address_visible) {
					return address.ToString(true);
				} else {
					return "*";
				}
			}
			std::string ToString(bool includeId = false) {
				std::ostringstream ss;
				ss << nick << "!";
				ss << username << "@";
				ss << address.ToString(true);
				if(includeId) {
					ss << "+" << id;
				}
				return ss.str();
			}
			std::string ToBase64String(bool includeId = false) {
				std::string s = ToString(includeId);
				const char *base64 = OS::BinToBase64Str((uint8_t *)s.c_str(), s.length());
				std::string b64_string = base64;
				free((void *)base64);
				return b64_string;
			}
			static UserSummary FromBase64String(std::string base64) {
				uint8_t *data_out;
				size_t data_len;

				OS::Base64StrToBin((const char *)base64.c_str(), &data_out, data_len);
				std::string summary = std::string((const char*)data_out, data_len);
				free(data_out);

				return UserSummary(summary);
			}
	};

	class UserAddressVisibiltyInfo : public OS::LinkedList<UserAddressVisibiltyInfo *> {
		public:
			UserAddressVisibiltyInfo(int user_id, int channel_id) : OS::LinkedList<UserAddressVisibiltyInfo *>() {
				this->channel_id = channel_id;
				this->user_id = user_id;
			}
			int channel_id;
			int user_id;
	};
	class IterateUserAddressVisibiltyInfoState {
		public:
			IterateUserAddressVisibiltyInfoState() {
				channel_id = 0;
				user_id = 0;
				peer = NULL;
				found = false;
			}
			int channel_id;
			int user_id;
			bool found;
			Peer *peer;
	};

	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();

		void OnConnectionReady();
		void Delete(bool timeout = false, std::string reason = "");
		void Delete(bool timeout = false); //required, due to INetPeer inheritance
		
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
		void OnRecvDirectMsg(UserSummary from, std::string msg, std::string type);
		int GetBackendId() { return m_user_details.id; };

		void send_numeric(int num, std::string str, bool no_colon = false, std::string target_name = "", bool append_name = true, std::string default_name = "*");
		void send_message(std::string messageType, std::string messageContent, UserSummary from = UserSummary(), std::string to = "", std::string target = "", bool no_colon = false);
		void handle_channel_join_events(ChannelSummary channel);

		int GetChannelFlags(int channel_id);
		void SetChannelFlags(int channel_id, int modeflags);
		
		std::vector<int> GetChannels();

		void SendNickUpdate(std::string newNick);
		int GetOperFlags() { return m_user_details.operflags; };

		void refresh_user_details(bool user_registration = false);

		void perform_keepalive();

		///
		/// This block is public for use in async tasks
		///
		void send_no_such_target_error(std::string channel);
		///

		void OnRemoteDisconnect(std::string reason);

		void OnSetExternalUserChanModeFlags(int user_id, int channel_id, int modeflags);

		bool IsUserAddressVisible(int user_id);
		void AddUserToAddressVisbilityList(int user_id, int channel_id); //user joined / ops gained
		void RemoveUserAddressVisibility_ByChannel(int channel_id); //PART / ops lost
		void RemoveUserAddressVisibility_ByChannelUser(int user_id, int channel_id); //user parted
		void RemoveUserAddressVisibility_ByUser(int user_id); //user quit
		void RefreshUserAddressVisibility_ByChannel(int channel_id);
		void PurgeUserAddressVisibility();
	private:
		static void m_oper_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer);
		static void m_login_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void* extra, INetPeer* peer);
		static void OnGetBackendId(TaskResponse response_data, Peer *peer);
		static void OnNickReserve(TaskResponse response_data, Peer *peer);
		static void OnUserRegistered(TaskResponse response_data, Peer *peer);
		static void OnNames_FetchChannelInfo(TaskResponse response_data, Peer *peer);
		static void OnMode_FetchChannelInfo(TaskResponse response_data, Peer* peer);
		static void OnMode_FetchUserInfo(TaskResponse response_data, Peer* peer);
		static void OnMode_UpdateUserMode(TaskResponse response_data, Peer* peer);
		static void OnMode_FetchBanInfo(TaskResponse response_data, Peer* peer);
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
		static void OnSetGroup(TaskResponse response_data, Peer* peer);
		static void OnWho_FetchChannelInfo(TaskResponse response_data, Peer *peer);
		static void OnWho_FetchUserInfo(TaskResponse response_data, Peer *peer);
		static void OnUsrip_FetchUser(TaskResponse response_data, Peer* peer);
		static void OnSetUserMode(TaskResponse response_data, Peer* peer);
		static void OnListUserModes(TaskResponse response_data, Peer* peer);
		static void OnDeleteUserMode(TaskResponse response_data, Peer* peer);
		static void OnRecievedGameInfo(TaskResponse response_data, void *extra);
		static void OnGetGameInfo_Crypt(TaskResponse response_data, Peer* peer);
		static void OnLookupGlobalUsermode(TaskResponse response_data, Peer *peer);
		static void OnListChanProps(TaskResponse response_data, Peer *peer);
		static void OnSetChanProps(TaskResponse response_data, Peer* peer);
		static void OnDelChanProps(TaskResponse response_data, Peer* peer);
		static void OnKillUser(TaskResponse response_data, Peer* peer);
		static void OnNickReserve_DuplicateRemoteKill(TaskResponse response_data, Peer *peer);
		static void OnFetch_LUsers(TaskResponse response_data, Peer* peer);

		void handle_nick(std::vector<std::string> data_parser);
		void handle_user(std::vector<std::string> data_parser);
		void handle_ping(std::vector<std::string> data_parser);
		void handle_oper(std::vector<std::string> data_parser);
		void handle_login(std::vector<std::string> data_parser);
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
		void handle_listlimit(std::vector<std::string> data_parser);
		void handle_whois(std::vector<std::string> data_parser);
		void handle_quit(std::vector<std::string> data_parser);
		void handle_setckey(std::vector<std::string> data_parser);
		void handle_getckey(std::vector<std::string> data_parser);
		void handle_setkey(std::vector<std::string> data_parser);
		void handle_getkey(std::vector<std::string> data_parser);
		void handle_setchankey(std::vector<std::string> data_parser);
		void handle_getchankey(std::vector<std::string> data_parser);
		void handle_setgroup(std::vector<std::string> data_parser);
		void handle_who(std::vector<std::string> data_parser);
		void handle_setusermode(std::vector<std::string> data_parser);
		void handle_listusermodes(std::vector<std::string> data_parser);
		void handle_delusermode(std::vector<std::string> data_parser);
		void handle_setchanprops(std::vector<std::string> data_parser);
		void handle_listchanprops(std::vector<std::string> data_parser);
		void handle_delchanprops(std::vector<std::string> data_parser);
		void handle_crypt(std::vector<std::string> data_parser);
		void handle_kill(std::vector<std::string> data_parser);		
		void handle_loginpreauth(std::vector<std::string> data_parser);
		void handle_lusers(std::vector<std::string> data_parser);

		void handle_channel_mode_command(std::vector<std::string> data_parser);
		void handle_user_mode_command(std::vector<std::string> data_parser);

		void send_topic(std::string channel);

		void send_flood_warning();

		int GetListUserCount(ChannelSummary summary);

		//UserAddressVisibiltyInfo stuff
		OS::LinkedListHead<UserAddressVisibiltyInfo *>* mp_user_address_visibility_list;
		static bool LLIterator_IsUserAddressVisible(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state);
		static bool LLIterator_RemoveUserAddressVisibility_ByChannel(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state);
		static bool LLIterator_RemoveUserAddressVisibility_ByChannelUser(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state);
		static bool LLIterator_RemoveUserAddressVisibility_ByUser(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state);
		static bool LLIterator_PurgeUserAddressVisibility(UserAddressVisibiltyInfo* info, IterateUserAddressVisibiltyInfoState* state);
		static void OnLookup_RefreshUserAddressVisibility_ByChannel(TaskResponse response_data, Peer *peer);
		//

		void handle_message_command(std::string type, std::vector<std::string> data_parser);
		void send_quit(std::string reason);
		static void getChannelSpecialInfo(std::ostringstream &ss, ChannelSummary summary);
		static void SerializeUsermodeRecord(UsermodeRecord record, std::ostringstream& ss);
		static void SerializeChanpropsRecord(ChanpropsRecord record, std::ostringstream& ss);

		void handle_ban_hostmask(std::string channel, std::string hostmask, bool set);

		void perform_oper_check();
		static void OnFetchOperStatus(TaskResponse response_data, Peer* peer);

		void perform_acquire_uniquenick(UserSummary target);

		OS::GameData m_game;
		
		struct timeval m_last_recv, m_last_ping, m_connect_time, m_last_keepalive;

		OS::User m_user;
		OS::Profile m_profile;

		UserSummary m_user_details;

		OS::CMutex *mp_mutex;

		std::vector<CommandEntry> m_commands;

		bool m_sent_client_init;

		std::map<int, int> m_channel_flags;


		bool m_using_encryption;
		bool m_stream_waiting;
		gs_crypt_key m_crypt_key_in;
		gs_crypt_key m_crypt_key_out;

		int m_flood_weight;
	};
}
#endif //_GPPEER_H