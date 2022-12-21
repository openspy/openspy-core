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
		EConnectionState_WaitApprovedResponse,
		EConnectionState_WaitRequest
	};


	/*
	* C->S "Game Client" mode msg ids
	*/
	enum EClientIncomingRequest {
		EClientIncomingRequest_ServerList,
		EClientIncomingRequest_MOTD
	};

	/*
	* C->S "Game Server" mode msg ids
	*/
	enum EServerIncomingRequest {
		EServerIncomingRequest_Heartbeat = 1,
		EServerIncomingRequest_StatsUpdate,
		EServerIncomingRequest_NewServer = 4
	};

	/*
	* S->C "Game Server" mode msg ids
	*/
	enum EServerOutgoingRequest {
		EServerOutgoingRequest_RequestHeartbeat = 0, //Request a heartbeat from the game server
		EServerOutgoingRequest_DetectedPorts = 1, // detected hb port info
		EServerOutgoingRequest_InformMatchId = 3, //Match ID for the server? or round?
		EServerOutgoingRequest_PackagesData,

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
			

			void send_challenge_response(std::string response);
			void send_challenge_authorization();
			void send_motd();
			void send_verified();
			void send_server_id(int id);
			void send_heartbeat_request(uint8_t id, uint32_t code);
			void send_detected_ports();
			/*
			* Protocol init handlers
			*/
			void handle_challenge_response(OS::Buffer buffer);
			void handle_uplink_info(OS::Buffer buffer); //"Game Server" mode only

			/*
			* "Game Server" command handlers
			*/
			void handle_newserver_request(OS::Buffer recv_buffer);			
			void handle_heartbeat(OS::Buffer buffer);
			

			/*
			* "Game Client" command handlers
			*/
			void handle_motd(OS::Buffer recv_buffer);
			void handle_request_server_list(OS::Buffer recv_buffer);


			static void on_get_server_list(MM::MMTaskResponse response);

			int get_server_flags(MM::ServerRecord record);
			int get_server_flags_ut2003(MM::ServerRecord record);

			void delete_server();

			//serialization stuff
			static int Read_CompactInt(OS::Buffer& buffer);
			static std::string Read_FString(OS::Buffer &buffer);
			static void Write_CompactInt(OS::Buffer& buffer, int value);
			static void Write_FString(std::string input, OS::Buffer &buffer);

			const CommandEntry* GetCommandByCode(uint8_t code);
			static const CommandEntry m_client_commands[];
			static const CommandEntry m_server_commands[];


			EConnectionState m_state;
			UT::Config *m_config;

			OS::Address m_server_address;

			uint32_t m_client_version;



			bool m_got_server_init;

	};
}
#endif //_GPPEER_H