#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"
#include "MMQuery.h"


namespace SB {
	class Driver;
	class Server;



	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V1Peer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved
		

		void informDeleteServers(MM::ServerListQuery servers);
		void informNewServers(MM::ServerListQuery servers);
		void informUpdateServers(MM::ServerListQuery servers);
	protected:
		void SendPacket(uint8_t *buff, int len, bool prepend_length);
		void handle_packet(char *data, int len);

	};
}
#endif //_SAMPRAKPEER_H