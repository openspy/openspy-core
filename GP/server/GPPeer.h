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

namespace GP {
	class Driver;
	typedef struct _PeerStats {
		int total_requests;
		int version;

		long long bytes_in;
		long long bytes_out;

		int packets_in;
		int packets_out;

		OS::Address m_address;
		OS::GameData from_game;

		bool disconnected;
	} PeerStats;
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

		int GetPing();
		void send_ping();

		void send_login_challenge(int type);
		void SendPacket(const uint8_t *buff, int len, bool attach_final = true);

		//event messages
		void send_add_buddy_request(int from_profileid, const char *reason);
		void send_authorize_add(int profileid, bool silent = false);

		void inform_status_update(int profileid, GPShared::GPStatus status, bool no_update = false);
		void send_revoke_message(int from_profileid, int date_unix_timestamp);
		void send_buddy_message(char type, int from_profileid, int timestamp, const char *msg);

		void send_user_blocked(int from_profileid);
		void send_user_block_deleted(int from_profileid);

		//
		static OS::MetricValue GetMetricItemFromStats(PeerStats stats);
		OS::MetricInstance GetMetrics();
		PeerStats GetPeerStats() { if (m_delete_flag) m_peer_stats.disconnected = true; return m_peer_stats; };
	private:
		void refresh_buddy_list();
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
		static void m_newuser_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);

		int m_search_operation_id;
		static void m_getprofile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		void handle_bm(OS::KVReader data_parser);

		void handle_addblock(OS::KVReader data_parser);
		void handle_removeblock(OS::KVReader data_parser);

		void handle_updatepro(OS::KVReader data_parser);
		void handle_updateui(OS::KVReader data_parser);

		void handle_keepalive(OS::KVReader data_parser);
		//

		//login
		void perform_nick_email_auth(const char *nick_email, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);
		void perform_uniquenick_auth(const char *uniquenick, int partnercode, int namespaceid, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);
		void perform_preauth_auth(const char *auth_token, const char *server_challenge, const char *client_challenge, const char *response, int operation_id, INetPeer *peer);

		static void m_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);
		static void m_buddy_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_block_list_lookup_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_create_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_delete_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);
		static void m_update_profile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);


		void send_buddies();
		void send_blocks();
		void send_error(GPShared::GPErrorCode code, std::string addon_data = "");
		void send_backend_auth_event();

		void ResetMetrics();

		OS::GameData m_game;
		OS::Buffer m_recv_buffer;
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
		PeerStats m_peer_stats;

		OS::CMutex *mp_mutex;
	};
}
#endif //_GPPEER_H