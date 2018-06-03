#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetIOInterface.h>

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
		INetIOSocket *getListenerSocket();

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		const std::vector<INetIOSocket *> getSockets();
		void SendDeleteServer(MM::Server *server);
	    void SendNewServer(MM::Server *server);
	    void SendUpdateServer(MM::Server *server);

		void AddDeleteServer(MM::Server serv);
		void AddNewServer(MM::Server serv);
		void AddUpdateServer(MM::Server serv);

		void debug_dump();

		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();
		void DeleteClients();

		int m_version;

		std::vector<Peer *> m_connections;

		struct timeval m_server_start;

		int m_sb_version;

		std::vector<SB::Peer *> m_peers_to_delete;

		//safe for now, until pointers one day get added
		std::queue<MM::Server> m_server_delete_queue;
		std::queue<MM::Server> m_server_new_queue;
		std::queue<MM::Server> m_server_update_queue;
		
		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		INetIOSocket *mp_socket;

	};
}
#endif //_SBDRIVER_H