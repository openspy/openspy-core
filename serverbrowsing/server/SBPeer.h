#ifndef _SBPEER_H
#define _SBPEER_H
#include "../main.h"
#include "MMQuery.h"

#define MAX_OUTGOING_REQUEST_SIZE (MAX_FIELD_LIST_LEN + MAX_FILTER_LEN + 255)

#define LIST_CHALLENGE_LEN 8





namespace SB {
	struct sServerListReq {
		uint8_t protocol_version;
		uint8_t encoding_version;
		uint32_t game_version;
		
		char challenge[LIST_CHALLENGE_LEN];
		
		const char *filter;
		//const char *field_list;
		std::vector<std::string> field_list;
		
		uint32_t source_ip; //not entire sure what this is for atm
		uint32_t max_results;
		
		bool send_groups;
		bool send_wan_ip;
		bool push_updates;
		bool no_server_list;
		
		OS::GameData m_for_game;
		OS::GameData m_from_game;

	};
	class Driver;
	class Server;

	enum EConnectionState {
		EConnectionState_NoInit,
	};

	struct sServerCache {
		char key[64];
		OS::Address wan_address;
		bool full_keys;
	};


	class Peer {
	public:
		Peer(Driver *driver, struct sockaddr_in *address_info, int sd);
		~Peer();
		
		virtual void send_ping() = 0;
		virtual void think(bool packet_waiting) = 0; //called when no data is recieved
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };



		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }

		bool serverMatchesLastReq(MM::Server *server, bool require_push_Flag = true);

		virtual void informDeleteServers(MM::ServerListQuery servers) = 0;
		virtual void informNewServers(MM::ServerListQuery servers) = 0;
		virtual void informUpdateServers(MM::ServerListQuery servers) = 0;
	protected:

		Driver *mp_driver;
		int m_sd;
		EConnectionState m_state;


		struct sockaddr_in m_address_info;

		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;



		OS::GameData m_game;		

		sServerListReq m_last_list_req;

		std::vector<sServerCache> m_visible_servers;
		void cacheServer(MM::Server *server);
		void DeleteServerFromCacheByIP(OS::Address address);
		sServerCache FindServerByIP(OS::Address address);

		virtual void SendPacket(uint8_t *buff, int len, bool prepend_length) = 0;

	};
}
#endif //_SAMPRAKPEER_H