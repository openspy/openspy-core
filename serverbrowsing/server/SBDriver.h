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
#define DRIVER_THREAD_TIME 1000

namespace SB {
	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, int version = 2);
		~Driver();
		void think(bool listen_waiting);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		const std::vector<int> getSockets();
		void SendDeleteServer(MM::Server *server);
	    void SendNewServer(MM::Server *server);
	    void SendUpdateServer(MM::Server *server);


		int GetNumConnections();

		bool HasPeer(SB::Peer * peer);

		void AddDeleteServer(MM::Server serv);
		void AddNewServer(MM::Server serv);
		void AddUpdateServer(MM::Server serv);

		OS::MetricInstance GetMetrics();
		void debug_dump();
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();

		int m_sd;
		int m_version;

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

		int m_sb_version;

		std::vector<SB::Peer *> m_peers_to_delete;

		//safe for now, until pointers one day get added
		std::queue<MM::Server> m_server_delete_queue;
		std::queue<MM::Server> m_server_new_queue;
		std::queue<MM::Server> m_server_update_queue;
		std::queue<PeerStats> m_stats_queue; //pending stats to be sent(deleted clients)
		
		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

	};

	extern Driver *g_gbl_driver;
}
#endif //_SBDRIVER_H