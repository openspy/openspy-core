#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "SBPeer.h"
#include "MMQuery.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define SB_PING_TIME 30

namespace SB {
	class Peer;

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
		
		void SendDeleteServer(MM::ServerListQuery servers);
	    void SendNewServer(MM::ServerListQuery servers);
	    void SendUpdateServer(MM::ServerListQuery servers);


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