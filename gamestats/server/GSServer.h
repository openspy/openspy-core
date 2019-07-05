#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>
#include <server/tasks/tasks.h>
namespace GS {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		TaskScheduler<PersistBackendRequest, TaskThreadData> *GetGamestatsTask() { return mp_gamestats_tasks; };
	private:
		TaskScheduler<PersistBackendRequest, TaskThreadData> *mp_gamestats_tasks;
	};
}
#endif //_GPSERVER_H