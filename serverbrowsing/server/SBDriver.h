#ifndef _SBDRIVER_H
#define _SBDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>
#include <OS/Net/NetIOInterface.h>
#include <OS/Net/drivers/TCPDriver.h>

#include "SBPeer.h"
#include "V2Peer.h"
#include "V1Peer.h"
#include <tasks/tasks.h>

#include <map>
#include <queue>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define SB_PING_TIME 15
#define DRIVER_THREAD_TIME 1000

namespace SB {
	class Peer;

	class Driver : public TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, int version = 2, bool proxyHeaders = false);
		void SendDeleteServer(MM::Server *server);
	    void SendNewServer(MM::Server *server);
	    void SendUpdateServer(MM::Server *server);

		void AddDeleteServer(MM::Server serv);
		void AddNewServer(MM::Server serv);
		void AddUpdateServer(MM::Server serv);
		void TickConnections();
		INetPeer *CreatePeer(INetIOSocket *socket);
	private:
		int m_version;


		//safe for now, until pointers one day get added
		std::queue<MM::Server> m_server_delete_queue;
		std::queue<MM::Server> m_server_new_queue;
		std::queue<MM::Server> m_server_update_queue;
	};
}
#endif //_SBDRIVER_H