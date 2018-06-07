#ifndef _V2PEER_H
#define _V2PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include <OS/Buffer.h>

#include <OS/gamespy/gsmsalg.h>

#include "MMPush.h"

#include <time.h>

#define REQUEST_KEY_LEN 4
#define QR2_PING_TIME 120

#define QR2_RESEND_MSG_TIME 5 //resend every 5 seconds
#define QR2_MAX_RESEND_COUNT 5
namespace QR {
	class Driver;

	class V2Peer : public Peer {
	public:
		V2Peer(Driver *driver, INetIOSocket *sd);
		~V2Peer();
		
		void think(bool listener_waiting);

		void handle_packet(INetIODatagram packet);

		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(void *data, size_t data_len);
		void OnGetGameInfo(OS::GameData game_info, void *extra);
		void OnRegisteredServer(int pk_id, void *extra);

		void ResendMessages();
	private:

		void SendClientMessage(OS::Buffer buffer, uint32_t key, bool no_insert = false);
		void handle_heartbeat(OS::Buffer &buffer);
		void handle_challenge(OS::Buffer &buffer);
		void handle_keepalive(OS::Buffer &buffer);
		void handle_available(OS::Buffer &buffer);
		void handle_client_message_ack(OS::Buffer &buffer);

		void SendPacket(OS::Buffer &buffer);
		void send_ping();
		void send_challenge();

		bool m_recv_instance_key;
		uint8_t m_instance_key[REQUEST_KEY_LEN];
		char m_challenge[CHALLENGE_LEN + 1];
		bool m_sent_challenge;

		std::map<uint32_t, OS::Buffer> m_client_message_queue;

		struct timeval m_last_msg_resend;
		int m_resend_count;
	};
}
#endif //_V2PEER_H