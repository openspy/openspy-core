#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
namespace MM {
	class UTMasterRequest;
}
namespace UT {
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void init();
		void tick();
		void shutdown();
		TaskScheduler<MM::UTMasterRequest, TaskThreadData> *getScheduler() { return mp_task_scheduler; };
        void doInternalLoadGameData(redisContext *ctx); //called by async task on startup
		private:
			TaskScheduler<MM::UTMasterRequest, TaskThreadData> *mp_task_scheduler;
	};
}
#endif //_SMSERVER_H
