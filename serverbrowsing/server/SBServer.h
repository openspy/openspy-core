#ifndef _SBSERVER_H
#define _SBSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/TaskPool.h>
#include <tasks/tasks.h>

class SBServer : public INetServer {
public:
	SBServer();
	~SBServer();
	void init();
	void tick();
	TaskScheduler<MM::MMQueryRequest, TaskThreadData> *getScheduler() { return mp_task_scheduler; };

	void OnNewServer(MM::Server server);
	void OnUpdateServer(MM::Server server);
	void OnDeleteServer(MM::Server server);
private:
	TaskScheduler<MM::MMQueryRequest, TaskThreadData> *mp_task_scheduler;
};
#endif //_SBSERVER_H