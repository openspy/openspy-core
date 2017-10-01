#ifndef _QRDRIVER_H
#define _QRDRIVER_H
#include <stdint.h>
#include <queue>
#include <OS/OpenSpy.h>
#include <OS/Mutex.h>
#include <OS/Net/NetDriver.h>

#include "QRPeer.h"


#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define MAX_DATA_SIZE 1400
namespace QR {
	class Peer;
	typedef struct _PeerStats PeerStats;


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
		Peer *find_or_create(struct sockaddr_in *address, int version = 2);

		const std::vector<int> getSockets();
		int GetNumConnections();

		const std::vector<INetPeer *> getPeers();
		OS::MetricInstance GetMetrics();
	private:

		void TickConnections();

		int m_sd;

		std::vector<Peer *> m_connections;
		std::vector<Peer *> m_peers_to_delete;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

		std::queue<PeerStats> m_stats_queue; //pending stats to be sent(deleted clients)

		OS::CMutex *mp_mutex;

	};
}
#endif //_QRDRIVER_H