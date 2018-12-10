#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include <OS/gamespy/gsmsalg.h>
#include <OS/KVReader.h>

#include <tasks/tasks.h>

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
		V1Peer(Driver *driver, INetIOSocket *socket);
		~V1Peer();
		
		void think(bool listener_waiting);

		void handle_packet(INetIODatagram packet);

		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(void *data, size_t data_len);
		void OnGetGameInfo(OS::GameData game_info, int state);
		void OnRegisteredServer(int pk_id);
	private:
		void SendPacket(std::string str, bool attach_final = true);
		void send_ping();
		void handle_echo(OS::KVReader data_parser);
		void handle_heartbeat(OS::KVReader data_parser);
		void handle_validate(OS::KVReader data_parser);
		void handle_ready_query_state(OS::KVReader data_parser);
		void parse_rules(OS::KVReader data_parser);
		void parse_players(OS::KVReader data_parser);

		uint8_t m_challenge[7];
		uint8_t m_ping_challenge[7];
		bool m_sent_challenge;

		bool m_uses_validation;
		bool m_validated; //passed "echo" validation or use "secure" response

		//used for queueing packets that require game info until gamedata available
		int m_waiting_gamedata;
		std::queue<INetIODatagram> m_waiting_packets;

		EV1ClientQueryState m_query_state;
	};
}
#endif //_V1PEER_H