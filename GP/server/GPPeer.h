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
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		void Delete();
		
		void think(bool packet_waiting);
		void handle_packet(char *data, int len);
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetProfileID();

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		int GetPing();
		void send_ping();

		void send_login_challenge(int type);
		void SendPacket(const uint8_t *buff, int len, bool attach_final = true);

		//event messages
		void send_add_buddy_request(int from_profileid, const char *reason);
		void send_authorize_add(int profileid);

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
		void handle_login(const char *data, int len);
		void handle_auth(const char *data, int len); //possibly for unexpected loss of connection to retain existing session

		void handle_status(const char *data, int len);
		void handle_statusinfo(const char *data, int len);

		void handle_addbuddy(const char *data, int len);
		void handle_delbuddy(const char *data, int len);
		void handle_revoke(const char *data, int len);
		void handle_authadd(const char *data, int len);

		void handle_pinvite(const char *data, int len);

		void handle_getprofile(const char *data, int len);

		void handle_newprofile(const char *data, int len);
		void handle_delprofile(const char *data, int len);

		void handle_registernick(const char *data, int len);
		void handle_registercdkey(const char *data, int len);

		int m_search_operation_id;
		static void m_getprofile_callback(OS::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer);

		void handle_bm(const char *data, int len);

		void handle_addblock(const char *data, int len);
		void handle_removeblock(const char *data, int len);

		void handle_updatepro(const char *data, int len);
		void handle_updateui(const char *data, int len);

		void handle_keepalive(const char *data, int len);
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
		void send_error(GPShared::GPErrorCode code);
		void send_backend_auth_event();

		void ResetMetrics();

		OS::GameData m_game;
		Driver *mp_driver;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		char m_challenge[CHALLENGE_LEN + 1];

		GPShared::GPStatus m_status;

		std::string m_backend_session_key; //session key

		//these are modified by other threads
		//std::vector<int> m_buddies;
		std::map<int, GPShared::GPStatus> m_buddies;
		std::vector<int> m_blocks;
		std::vector<int> m_blocked_by;

		OS::User m_user;
		OS::Profile m_profile;
		PeerStats m_peer_stats;
	};
}
#endif //_GPPEER_H