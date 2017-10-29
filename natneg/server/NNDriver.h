#ifndef _QRDRIVER_H
#define _QRDRIVER_H
#include <stdint.h>
#include <OS/Net/NetDriver.h>

#include "NNPeer.h"


#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define MAX_DATA_SIZE 1400
namespace NN {
	class Peer;



	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool packet_waiting);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		Peer *find_or_create(struct sockaddr_in *address);

		void OnGotCookie(int cookie, int client_idx, OS::Address address);

		const std::vector<INetPeer *> getPeers();
		const std::vector<int> getSockets();
		OS::MetricInstance GetMetrics();
	private:

		void TickConnections();

		int m_sd;

		std::vector<Peer *> m_connections;
		std::vector<Peer *> m_peers_to_delete;

		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;
	};
}
#endif //_QRDRIVER_H