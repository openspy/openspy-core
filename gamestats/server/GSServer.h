#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include "GSBackend.h"
#define STATS_SERVER_PORT 29920
namespace GS {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		void SetTaskPool(OS::TaskPool<GSBackend::PersistBackendTask, GSBackend::PersistBackendRequest> *pool);
		OS::MetricInstance GetMetrics();
	private:
		struct timeval m_last_analytics_submit_time;
	};
}
#endif //_GPSERVER_H