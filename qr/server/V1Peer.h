#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"

#include "QRDriver.h"
#include "QRPeer.h"

#include <OS/legacy/gsmsalg.h>

#include "MMPush.h"

namespace QR {
	class Driver;

	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V1Peer();
		
		void think(bool packet_waiting);

		void handle_packet(char *recvbuf, int len);

		void send_error(bool die, const char *fmt, ...);
		void SendClientMessage(uint8_t *data, int data_len);
	private:
	};
}
#endif //_V1PEER_H