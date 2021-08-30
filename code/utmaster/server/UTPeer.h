#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"

#include <OS/Net/NetPeer.h>

#define UTMASTER_PING_TIME 120


namespace UT {
	enum EConnectionState {
		EConnectionState_WaitChallengeResponse,
		EConnectionState_ApprovedResponse,
		EConnectionState_WaitRequest,
		EConnectionState_Heartbeat
	};

	enum ERequestType {
		ERequestType_ServerList,
		ERequestType_MOTD,
		ERequestType_NewServer = 4,
	};

	class Driver;
	class Peer : public INetPeer {
	public:
		Peer(Driver *driver, INetIOSocket *sd);
		~Peer();
		
		void OnConnectionReady();
		void think(bool packet_waiting);
		void handle_packet(std::string data);

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		void handle_packet(OS::Buffer recv_buffer);

		void send_challenge(std::string challenge_string);
		void send_packet(OS::Buffer buffer);

		void send_challenge_authorization();
		void send_motd();
		void send_verified();
		void handle_request_server_list(OS::Buffer recv_buffer);

		void send_keys_request();



		void Delete(bool timeout = false);
		private:
		EConnectionState m_state;
		bool m_is_server;

		void handle_challenge_response(OS::Buffer buffer);
		void handle_heartbeat(OS::Buffer buffer);

		//serialization stuff
		std::string Read_FString(OS::Buffer &buffer);
		void Write_FString(std::string input, OS::Buffer &buffer);

	};
}
#endif //_GPPEER_H