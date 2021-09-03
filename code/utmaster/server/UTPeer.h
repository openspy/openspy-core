#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"

#include <OS/Net/NetPeer.h>

#define UTMASTER_PING_TIME 120

namespace MM {
	class MMTaskResponse;
	class ServerRecord;
}

namespace UT {
	enum EConnectionState {
		EConnectionState_WaitChallengeResponse,
		EConnectionState_ApprovedResponse,
		EConnectionState_WaitRequest,
		EConnectionState_Heartbeat
	};

	#define HEARTBEAT_MODE_OFFSET 9000
	enum ERequestType {
		ERequestType_ServerList,
		ERequestType_MOTD,
		ERequestType_NewServer = 4,

		//server host mode requests
		ERequestType_Heartbeat = 1 + HEARTBEAT_MODE_OFFSET,
		ERequestType_PlayerQuery = 2 + HEARTBEAT_MODE_OFFSET,
	};

	class Driver;
	class Config;

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


		void send_challenge_response(std::string response);
		void send_challenge_authorization();
		void send_motd();
		void send_verified();
		void send_server_id(int id);

		void handle_request_server_list(OS::Buffer recv_buffer);
		void handle_newserver_request(OS::Buffer recv_buffer);

		static void on_get_server_list(MM::MMTaskResponse response);
		

		void Delete(bool timeout = false);
		int GetGameId();
		private:
			EConnectionState m_state;
			void handle_challenge_response(OS::Buffer buffer);
			void handle_heartbeat(OS::Buffer buffer);

			void delete_server();

			//serialization stuff
			static std::string Read_FString(OS::Buffer &buffer);
			static void Write_FString(std::string input, OS::Buffer &buffer);

			UT::Config *m_config;

			OS::Address m_server_address;

	};
}
#endif //_GPPEER_H