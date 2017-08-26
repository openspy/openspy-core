#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include <OS/legacy/gsmsalg.h>

#include "MMPush.h"

#define MAX_OUTGOING_REQUEST_SIZE 1024
#define QR1_PING_TIME 300
namespace QR {
	class Driver;

	enum EV1ClientQueryState {
		EV1_CQS_Basic,
		EV1_CQS_Info,
		EV1_CQS_Players,
		EV1_CQS_Rules,
		EV1_CQS_Complete,	
	};

	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V1Peer();
		
		void think(bool listener_waiting);

		void handle_packet(char *recvbuf, int len);

		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(uint8_t *data, int data_len);
		void OnGetGameInfo(OS::GameData game_info, void *extra);
	private:
		void SendPacket(const uint8_t *buff, int len, bool attach_final);
		void send_ping();
		void handle_echo(char *recvbuf, int len);
		void handle_heartbeat(char *recvbuf, int len);
		void handle_ready_query_state(char *recvbuf, int len);
		void parse_rules(char *recvbuf, int len);
		void parse_players(char *recvbuf, int len);

		void Delete();

		uint8_t m_challenge[7];
		uint8_t m_ping_challenge[7];
		bool m_sent_challenge;

		bool m_uses_validation;
		bool m_validated; //passed "echo" validation or use "secure" response

		bool m_pushed_server;

		EV1ClientQueryState m_query_state;
	};
}
#endif //_V1PEER_H