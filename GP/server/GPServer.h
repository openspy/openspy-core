#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include "GPBackend.h"
#include <OS/Net/NetServer.h>
#include <OS/TaskPool.h>
#define GP_SERVER_PORT 29900
namespace GP {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		void SetTaskPool(OS::TaskPool<GPBackend::GPBackendRedisTask, GPBackend::GPBackendRedisRequest> *pool);
		INetPeer *findPeerByProfile(int profile_id);
		void InformStatusUpdate(int from_profileid, GPShared::GPStatus status);
	private:
	};
}
#endif //_GPSERVER_H