#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include "MMPush.h"

namespace QR {
	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V1Peer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		void handle_packet(char *recvbuf, int len);


		void SendPacket(const uint8_t *buff, int len);

		int GetSocket() { return m_sd; };

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		void send_error(const char *msg, bool die = true);
		void SendClientMessage(uint8_t *data, int data_len);
	private:

		bool isTeamString(const char *string); //maybe move into peer

		void handle_heartbeat(char *buff, int len);

	};
}
#endif //_V2PEER_H