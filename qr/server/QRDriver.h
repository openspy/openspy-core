#ifndef _QRDRIVER_H
#define _QRDRIVER_H
#include <stdint.h>
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
		Peer *find_or_create(struct sockaddr_in *address, int version = 2);

		int setup_fdset(fd_set *fdset);


		int GetNumConnections();

	private:

		void TickConnections(fd_set *fdset);

		int m_sd;

		std::vector<Peer *> m_connections;
		std::vector<Peer *> m_peers_to_delete;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

	};
}
#endif //_QRDRIVER_H