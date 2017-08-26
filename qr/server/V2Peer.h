#ifndef _V2PEER_H
#define _V2PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include <OS/legacy/gsmsalg.h>

#include "MMPush.h"

#define REQUEST_KEY_LEN 4
#define QR2_PING_TIME 120
namespace QR {
	class Driver;

	class V2Peer : public Peer {
	public:
		V2Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V2Peer();
		
		void think(bool listener_waiting);

		void handle_packet(char *recvbuf, int len);

		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(uint8_t *data, int data_len);
		void OnGetGameInfo(OS::GameData game_info, void *extra);
	private:
		void handle_heartbeat(char *buff, int len);
		void handle_challenge(char *buff, int len);
		void handle_keepalive(char *buff, int len);
		void handle_available(char *buff, int len);

		void SendPacket(const uint8_t *buff, int len);
		void send_ping();
		void send_challenge();

		void Delete();

		bool m_recv_instance_key;
		uint8_t m_instance_key[REQUEST_KEY_LEN];
		char m_challenge[CHALLENGE_LEN + 1];
		bool m_sent_challenge;
	};
}
#endif //_V2PEER_H