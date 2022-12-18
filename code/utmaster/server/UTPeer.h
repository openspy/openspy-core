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
		EConnectionState_ApprovedResponse, //UT
		EConnectionState_WaitRequest
	};


	enum EClientModeRequest {
		EClientModeRequest_ServerList,
		EClientModeRequest_MOTD
	};

	enum EServerModeRequest {
		EServerModeRequest_ServerInit,
		EServerModeRequest_Heartbeat,
		EServerModeRequest_PlayerQuery,
		EServerModeRequest_NewServer = 4
	};


	class Peer;
	typedef void(Peer::*RequestHandler)(OS::Buffer recv_buffer);
	class CommandEntry {
	public:
		CommandEntry(uint8_t code, RequestHandler callback) {
			this->callback = callback;
			this->code = code;
		}
		uint8_t code;
		RequestHandler callback;
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

		void send_ping();

		

		void Delete(bool timeout = false);
		int GetGameId();
		private:
			EConnectionState m_state;
			void handle_challenge_response(OS::Buffer buffer);
			void handle_heartbeat(OS::Buffer buffer);

			void send_challenge_response(std::string response);
			void send_challenge_authorization();
			void send_motd();
			void send_verified();
			void send_server_id(int id);

			void handle_request_server_list(OS::Buffer recv_buffer);
			void handle_newserver_request(OS::Buffer recv_buffer);
			void handle_player_query(OS::Buffer recv_buffer);

			void handle_motd(OS::Buffer recv_buffer);

			void handle_server_init(OS::Buffer recv_buffer);


			int get_server_flags(MM::ServerRecord record);
			int get_server_flags_ut2003(MM::ServerRecord record);

			static void on_get_server_list(MM::MMTaskResponse response);

			void delete_server();

			//serialization stuff
			static int Read_CompactInt(OS::Buffer& buffer);
			static std::string Read_FString(OS::Buffer &buffer);
			static void Write_CompactInt(OS::Buffer& buffer, int value);
			static void Write_FString(std::string input, OS::Buffer &buffer);

			UT::Config *m_config;

			OS::Address m_server_address;

			uint32_t m_client_version;

			const CommandEntry *GetCommandByCode(uint8_t code);

			static const CommandEntry m_client_commands[];
			static const CommandEntry m_server_commands[];

			bool m_got_server_init;

	};
}
#endif //_GPPEER_H