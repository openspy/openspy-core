#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/OpenSpy.h>
#include <OS/TaskPool.h>
#include <OS/Net/NetServer.h>
#include <OS/Analytics/Metric.h>

#include "structs.h"

#define NATNEG_PORT 27901
namespace NN {
	class NNQueryTask;
	class Peer;
	class NNConnectionSummary;
	typedef struct _NNBackendRequest NNBackendRequest;
	class Server : public INetServer {
		public:
			Server();
			void init();
			void tick();
			void shutdown();
			void SetTaskPool(OS::TaskPool<NN::NNQueryTask, NN::NNBackendRequest> *pool);
			std::vector<NN::Peer *> FindConnections(NNCookieType cookie, int client_idx, bool inc_ref = false);
			OS::MetricInstance GetMetrics();
		private:
			struct timeval m_last_analytics_submit_time;
	};
}
#endif //_CHCGAMESERVER_H