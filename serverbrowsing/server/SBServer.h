#ifndef _SBSERVER_H
#define _SBSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/TaskPool.h>
#include "MMQuery.h"

class SBServer : public INetServer {
public:
	SBServer();
	~SBServer();
	void init();
	void tick();
	void SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool);

	void debug_dump();
private:
	OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *mp_task_pool;
};
#endif //_CHCGAMESERVER_H