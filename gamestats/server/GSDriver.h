#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "GSPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <OS/GPShared.h>

#define GP_PING_TIME (600)
#define DRIVER_THREAD_TIME 1000

namespace GS {
	class Peer;
	class Driver;
	extern Driver *g_gbl_gp_driver;
	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool listener_waiting);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		Peer *find_or_create(struct sockaddr_in *address);

		bool HasPeer(Peer *);
		Peer *FindPeerByProfileID(int profileid);

		int GetNumConnections();

		const std::vector<int> getSockets();
		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		OS::MetricInstance GetMetrics();
	private:
		void TickConnections();

		std::queue<PeerStats> m_stats_queue; //pending stats to be sent(deleted clients)

		int m_sd;

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

		static void *TaskThread(OS::CThread *thread);
		std::vector<GS::Peer *> m_peers_to_delete;
		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

	};
}
#endif //_SBDRIVER_H