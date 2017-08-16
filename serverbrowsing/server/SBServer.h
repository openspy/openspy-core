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
	void shutdown();
	void SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool);
private:
	
};
#endif //_CHCGAMESERVER_H