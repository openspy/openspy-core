#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/OpenSpy.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/Net/NetServer.h>

#include <tasks/tasks.h>

#include "structs.h"

#define NATNEG_PORT 27901
namespace NN {
	class NNQueryTask;
	class Peer;
	class NNConnectionSummary;
	class NNBackendRequest;
	class Server : public INetServer {
		public:
			Server();
			void init();
			void tick();
			void shutdown();
			std::vector<NN::Peer *> FindConnections(NNCookieType cookie, int client_idx, bool inc_ref = false);
			TaskScheduler<NNRequestData, TaskThreadData> *getScheduler() { return mp_task_scheduler; };
		private:
			TaskScheduler<NNRequestData, TaskThreadData> *mp_task_scheduler;
	};
}
#endif //_CHCGAMESERVER_H