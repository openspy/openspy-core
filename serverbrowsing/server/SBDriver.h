#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "SBPeer.h"
#include "V2Peer.h"
#include "V1Peer.h"
#include "MMQuery.h"

#include <map>
#include <queue>
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
		Driver(INetServer *server, const char *host, uint16_t port, int version = 2);
		~Driver();
		void tick(fd_set *fdset);
		void think(fd_set *fdset);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);

		int setup_fdset(fd_set *fdset);
		
		void SendDeleteServer(MM::Server *server);
	    void SendNewServer(MM::Server *server);
	    void SendUpdateServer(MM::Server *server);


		int GetNumConnections();

		bool HasPeer(SB::Peer * peer);
	private:

		void TickConnections(fd_set *fdset);

		int m_sd;
		int m_version;

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

		int m_sb_version;

		std::vector<SB::Peer *> m_peers_to_delete;
		
		OS::CMutex *mp_mutex;

	};

	extern Driver *g_gbl_driver;
}
#endif //_SBDRIVER_H