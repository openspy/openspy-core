#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"
#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Search/Profile.h>
#include <OS/Search/User.h>

#include <OS/GPShared.h>

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

#include <OS/SharedTasks/Auth/AuthTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>

// Extended message support
#define GPI_NEW_AUTH_NOTIFICATION	(1<<0)
#define GPI_NEW_REVOKE_NOTIFICATION (1<<1)

// New Status Info support
#define GPI_NEW_STATUS_NOTIFICATION (1<<2)

// Buddy List + Block List retrieval on login
#define GPI_NEW_LIST_RETRIEVAL_ON_LOGIN (1<<3)

// Remote Auth logins now return namespaceid/partnerid on login
// so the input to gpInitialize is ignored
#define GPI_REMOTEAUTH_IDS_NOTIFICATION (1<<4)

// New CD Key registration style as opposed to using product ids
#define GPI_NEW_CDKEY_REGISTRATION (1<<5)

#define MAX_UNPROCESSED_DATA 5000

using namespace GPShared;

#define LoadParamInt(read_name, write_var, var_name) if(var_name.HasKey(read_name)) { \
		write_var = var_name.GetValueInt(read_name); \
	} else { \
		write_var = 0;\
	}

#define LoadParam(read_name, write_var, var_name) if(var_name.HasKey(read_name)) { \
		write_var = var_name.GetValue(read_name); \
	}


namespace GP {
	class Driver;

	class Peer;
	typedef void(Peer::*CommandCallback)(OS::KVReader);
	class CommandEntry {
	public:
		CommandEntry(std::string name, bool login_required, CommandCallback callback) {
			this->name = name;
			this->login_required = login_required;
			this->callback = callback;
		}
		std::string name;
		bool login_required;
		CommandCallback callback;
	};


	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();
		void Delete(bool timeout = false);
		
		void think(bool packet_waiting);
		void handle_packet(std::string packet);

		int GetProfileID();

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void send_ping();

		void send_login_challenge(int type);
		void SendPacket(const uint8_t *buff, size_t len, bool attach_final = true);

		void RegisterCommands();

		//event messages
		void send_add_buddy_request(int from_profileid, const char *reason);
		void send_authorize_add(int from_profileid, int to_profileid, bool silent = false);

		void inform_status_update(int profileid, GPShared::GPStatus status, bool no_update = false);
		void send_revoke_message(int from_profileid, int date_unix_timestamp);
		void send_buddy_message(char type, int from_profileid, int timestamp, const char *msg);

		void send_user_blocked(int from_profileid);
		void send_user_block_deleted(int from_profileid);

		void send_list_status();

	private:
		//packet handlers
		void handle_login(OS::KVReader data_parser);
		void handle_auth(OS::KVReader data_parser); //possibly for unexpected loss of connection to retain existing session

		void handle_status(OS::KVReader data_parser);
		void handle_statusinfo(OS::KVReader data_parser);

		void handle_addbuddy(OS::KVReader data_parser);
		void handle_delbuddy(OS::KVReader data_parser);
		void handle_revoke(OS::KVReader data_parser);
		void handle_authadd(OS::KVReader data_parser);

		void handle_pinvite(OS::KVReader data_parser);

		void handle_getprofile(OS::KVReader data_parser);

		void handle_newprofile(OS::KVReader data_parser);
		void handle_delprofile(OS::KVReader data_parser);

		void handle_registernick(OS::KVReader data_parser);
		void handle_registercdkey(OS::KVReader data_parser);

		void handle_newuser(OS::KVReader data_parser);
		static void m_newuser_cb(bool success, OS::User user, OS::Profile profile, TaskShared::UserRegisterData auth_data, void *extra, INetPeer *peer);

		static void m_getprofile_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		void handle_bm(OS::KVReader data_parser);

		void handle_addblock(OS::KVReader data_parser);
		void handle_removeblock(OS::KVReader data_parser);

		void handle_updatepro(OS::KVReader data_parser);
		void handle_updateui(OS::KVReader data_parser);

		void handle_keepalive(OS::KVReader data_parser);
		void handle_logout(OS::KVReader data_parser);
		//

		//login
		void perform_nick_email_auth(const char *nick_email, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);
		void perform_uniquenick_auth(const char *uniquenick, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);
		void perform_preauth_auth(const char *auth_token, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);

		static void m_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer);
		static void m_buddy_list_lookup_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer);
		static void m_block_list_lookup_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, std::map<int, GPShared::GPStatus> status_map, void *extra, INetPeer *peer);
		static void m_create_profile_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_delete_profile_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_update_profile_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);


		void send_buddies();
		void send_blocks();
		bool m_got_buddies;
		bool m_got_blocks;
		void send_error(GPShared::GPErrorCode code, std::string addon_data = "");
		void send_backend_auth_event();

		OS::GameData m_game;
		Driver *mp_driver;

		struct timeval m_last_recv, m_last_ping, m_status_refresh;

		bool m_delete_flag;
		bool m_timeout_flag;

		char m_challenge[CHALLENGE_LEN + 1];

		GPShared::GPStatus m_status;

		std::string m_backend_session_key; //session key
		std::string m_kv_accumulator;

		//these are modified by other threads
		//std::vector<int> m_buddies;
		std::map<int, GPShared::GPStatus> m_buddies;
		std::vector<int> m_blocks;
		std::vector<int> m_blocked_by;

		OS::User m_user;
		OS::Profile m_profile;

		OS::CMutex *mp_mutex;

		std::vector<CommandEntry> m_commands;

		//used to add someone to your buddy list, after you accepted them. Client sends "authadd" in response to "addbuddyresposne" - allowing the user to be added to the lists
		std::map<int, timeval> m_authorized_adds;
	};
}
#endif //_GPPEER_H