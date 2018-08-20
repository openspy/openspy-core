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

#define PACKET_QUERY              0x00  //S -> C
#define PACKET_CHALLENGE          0x01  //S -> C
#define PACKET_ECHO               0x02  //S -> C (purpose..?)
#define PACKET_ECHO_RESPONSE      0x05  // 0x05, not 0x03 (order) | C -> S
#define PACKET_HEARTBEAT          0x03  //C -> S
#define PACKET_ADDERROR           0x04  //S -> C
#define PACKET_CLIENT_MESSAGE     0x06  //S -> C
#define PACKET_CLIENT_MESSAGE_ACK 0x07  //C -> S
#define PACKET_KEEPALIVE          0x08  //S -> C | C -> S
#define PACKET_PREQUERY_IP_VERIFY 0x09  //S -> C
#define PACKET_AVAILABLE          0x09  //C -> S
#define PACKET_CLIENT_REGISTERED  0x0A  //S -> C

#define QR2_OPTION_USE_QUERY_CHALLENGE 128 //backend flag

#define QR_MAGIC_1 0xFE
#define QR_MAGIC_2 0xFD


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