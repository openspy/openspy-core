#ifndef _SCHEDULEDTASK_H
#define _SCHEDULEDTASK_H
#include <OS/OpenSpy.h>
#include <OS/Net/NetServer.h>
#include <string>
#include <map>

#define SCHEDULED_TASK_WAIT_TIME 500

class TaskThreadData {
	public:
		redisOptions redis_options;
		redisContext *mp_redis_connection;
		INetServer *server;
		int id; //task thread index
};
#endif //_SCHEDULEDTASK_H
