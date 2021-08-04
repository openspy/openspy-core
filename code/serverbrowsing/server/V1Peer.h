#ifndef _V1PEER_H
#define _V1PEER_H
#include "../main.h"
#include "gutil.h"
#include <tasks/tasks.h>

#define MAX_UNPROCESSED_DATA 5000


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
		void OnConnectionReady();
	protected:

		void OnRetrievedServers(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);
		void OnRetrievedServerInfo(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);
		void OnRetrievedGroups(const MM::MMQueryRequest request, MM::ServerListQuery results, void *extra);
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
		void send_crypt_header(int enctype, OS::Buffer &buffer);
		void send_validation();
		std::string field_cleanup(std::string s);
		std::string skip_queryid(std::string s);

		std::string m_validation;
		uint8_t m_challenge[7];
		uint8_t m_enctype;
		crypt_key m_crypt_key_enctype2;
		bool m_validated;
		bool m_sent_crypt_key;


		int m_waiting_gamedata; //1 = waiting, 2 = process packet queue
		std::queue<std::string> m_waiting_packets;

		bool m_sent_validation;

		std::string m_kv_accumulator;
	};
}
#endif //_SAMPRAKPEER_H