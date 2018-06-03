#ifndef _NNDRIVER_H
#define _NNDRIVER_H
#include <stdint.h>
#include <OS/Net/NetDriver.h>

#include <queue>
#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define MAX_DATA_SIZE 1400
#define DRIVER_THREAD_TIME 1000
namespace NN {
	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		~Driver();
		void think(bool packet_waiting);

		Peer *find_client(OS::Address address, NNCookieType cookie = 0, bool use_client_info = false);
		Peer *find_client(NNCookieType cookie, int client_idx);
		std::vector<Peer *> find_clients(NNCookieType cookie, int client_idx, bool inc_ref = false);
		Peer *find_or_create(OS::Address address, INetIOSocket *socket, NNCookieType cookie);

		void get_packet_cookie(INetIODatagram dgram, NNCookieType &cookie);

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();

		int m_sd;

		std::vector<Peer *> m_connections;
		std::vector<Peer *> m_peers_to_delete;

		struct timeval m_server_start;

		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		INetIOSocket *mp_socket;
	};
}
#endif //_NNDRIVER_H