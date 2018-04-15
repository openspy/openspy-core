#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "SMPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif
#define DRIVER_THREAD_TIME 1000
namespace SM {
	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool listener_waiting);

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;

		OS::MetricInstance GetMetrics();
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();

		std::vector<SM::Peer *> m_peers_to_delete;

		//safe for now, until pointers one day get added
		std::queue<PeerStats> m_stats_queue; //pending stats to be sent(deleted clients)

		std::vector<Peer *> m_connections;

		struct timeval m_server_start;

		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		INetIOSocket *mp_socket;
	};
}
#endif //_SBDRIVER_H