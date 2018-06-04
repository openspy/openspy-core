#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"
#include "MMQuery.h"


namespace SB {
	class Driver;



	class V1Peer : public Peer {
	public:
		V1Peer(Driver *driver, INetIOSocket *sd);
		~V1Peer();
		
		void send_ping();
		void think(bool packet_waiting); //called when no data is recieved
		

		void informDeleteServers(MM::Server *server);
		void informNewServers(MM::Server *server);
		void informUpdateServers(MM::Server *server);
	protected:

		void OnRetrievedServers(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra);
		void OnRetrievedServerInfo(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra);
		void OnRetrievedGroups(const struct MM::_MMQueryRequest request, struct MM::ServerListQuery results, void *extra);
		void OnRecievedGameInfo(const OS::GameData game_data, void *extra);
		void OnRecievedGameInfoPair(const OS::GameData game_data_first, const OS::GameData game_data_second, void *extra);


		void SendPacket(const uint8_t *buff, size_t len, bool attach_final, bool skip_encryption = false);
		void SendServers(MM::ServerListQuery results);
		void SendServerInfo(MM::ServerListQuery results);
		void SendGroups(MM::ServerListQuery results);
		void handle_packet(std::string data);
		void handle_gamename(std::string data);
		void handle_list(std::string data);
		void send_error(bool disconnect, const char *fmt, ...);
		void send_crypt_header(int enctype);
		std::string field_cleanup(std::string s);

		std::string m_validation;
		uint8_t m_challenge[7];
		uint8_t m_enctype;
		unsigned int m_cryptkey_enctype2[326];
		unsigned char *m_keyptr;
		bool m_validated;


		int m_waiting_gamedata; //1 = waiting, 2 = process packet queue
		std::queue<std::string> m_waiting_packets;
	};
}
#endif //_SAMPRAKPEER_H