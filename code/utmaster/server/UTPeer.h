#ifndef _GPPEER_H
#define _GPPEER_H
#include "../main.h"

#include <OS/Net/NetPeer.h>

#define UTMASTER_PING_TIME 120


//SERVET TO CLIENT
#define MESSAGE_CHALLENGE 0x0b
#define MESSAGE_CHALLENGE_AUTHORIZATION 0x0e
#define MESSAGE_VERIFIED 0x0a
#define MESSAGE_NEWS 0xb0
#define MESSAGE_REQUEST_SERVER_LIST 0x2b

//CLIENT TO SERVER
#define MESSAGE_CHALLENGE_RESPONSE 0x68
#define MESSAGE_VERIFICATION_DATA 0x22
#define MESSAGE_NEWS_REQUEST 0x01

namespace UT {
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
		void send_packet(char type, OS::Buffer buffer);

		void send_challenge_authorization();
		void send_news();
		void send_verified();
		void handle_request_server_list(OS::Buffer recv_buffer);


		void Delete(bool timeout = false);
	private:


	};
}
#endif //_GPPEER_H