#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"
#include "MMQuery.h"


namespace SB {
	class Driver;



	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~V1Peer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved
		

		void informDeleteServers(MM::Server *server);
		void informNewServers(MM::Server *server);
		void informUpdateServers(MM::Server *server);
	protected:
		void SendPacket(const uint8_t *buff, int len, bool attach_final);
		void SendServers(MM::ServerListQuery results);
		void SendServerInfo(MM::ServerListQuery results);
		void SendGroups(MM::ServerListQuery results);
		void send_error(const char *msg, bool disconnect = true);
		void handle_packet(char *data, int len);
		void handle_gamename(char *data, int len);
		void handle_list(char *data, int len);
		void send_error(bool disconnect, const char *fmt, ...);
		void send_crypt_header(int enctype);
		std::string field_cleanup(std::string s);
		uint8_t m_challenge[7];
		uint8_t m_enctype;
		unsigned int m_cryptkey_enctype2[326];
		unsigned char *m_keyptr;
		bool m_validated;

	};
}
#endif //_SAMPRAKPEER_H