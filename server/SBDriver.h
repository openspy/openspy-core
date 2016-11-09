#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include "../NetDriver.h"

#include "SBPeer.h"
#include "../MMQuery.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif


namespace SB {
	class Peer;

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
		bool send_push_keys;
		bool no_server_list;
		
		OS::GameData m_for_game;
		OS::GameData m_from_game;

	};


	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void tick(fd_set *fdset);
		void think(fd_set *fdset);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		Peer *find_or_create(struct sockaddr_in *address);

		int setup_fdset(fd_set *fdset);
		
		


		int GetNumConnections();

	private:

		void TickConnections(fd_set *fdset);

		int m_sd;

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

	};
}
#endif //_SBDRIVER_H