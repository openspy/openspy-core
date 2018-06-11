#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"

#include <OS/Net/NetPeer.h>
#include <OS/Ref.h>

#include <OS/Auth.h>
#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Buffer.h>
#include <OS/Search/Profile.h>
#include <OS/Search/User.h>

#include <OS/GPShared.h>


#include "GSBackend.h"

#include <OS/KVReader.h>

#define MAX_UNPROCESSED_DATA 5000

namespace GS {
	typedef struct {
		int profileid;
		int operation_id;
	} GPPersistRequestData;
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
	class Driver;

	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();
		
		void think(bool packet_waiting);
		void handle_packet(std::string packet_string);
		void Delete(bool timeout = false);

		int GetProfileID();

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void send_ping();

		void send_login_challenge(int type);
		void SendPacket(std::string str, bool attach_final = true);
		void SendPacket(OS::Buffer &buffer, bool attach_final = true);

		OS::GameData GetGame() { return m_game; };
	private:
		//packet handlers
		static void newGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra);
		void handle_newgame(OS::KVReader data_parser);

		static void updateGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra);
		void handle_updgame(OS::KVReader data_parser);

		static void onGetGameDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra);
		void handle_authp(OS::KVReader data_parser);
		void handle_auth(OS::KVReader data_parser);
		void handle_getpid(OS::KVReader data_parser);

		static void getPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra);
		void handle_getpd(OS::KVReader data_parser);

		static void setPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra);
		void handle_setpd(OS::KVReader data_parser);

		//login
		void perform_preauth_auth(std::string auth_token, const char *response, int operation_id);
		void perform_pid_auth(int profileid, const char *response, int operation_id);
		static void m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer);


		void send_error(GPShared::GPErrorCode code);
		void gamespy3dxor(char *data, size_t len);
		int gs_chresp_num(const char *challenge);
		void gs_sesskey(int sesskey, char *out);
		bool IsResponseValid(const char *response);

		OS::GameData m_game;
		
		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		char m_challenge[CHALLENGE_LEN + 1];
		int m_session_key;


		//these are modified by other threads
		std::string m_backend_session_key;
		OS::User m_user;
		OS::Profile m_profile;

		OS::CMutex *mp_mutex;

		uint16_t m_game_port;

		std::string m_current_game_identifier;
		std::string m_response;

		std::string m_kv_accumulator;
	};
}
#endif //_GPPEER_H