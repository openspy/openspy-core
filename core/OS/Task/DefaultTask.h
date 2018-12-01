#ifndef _DEFAULTTASK_H
#define _DEFAULTTASK_H
#include <OS/OpenSpy.h>
#include <OS/Redis.h>
#include <OS/MessageQueue/MQInterface.h>
#include <OS/Net/NetServer.h>
#include <OS/Task.h>
#include <string>
#include <map>

class TaskThreadData {
	public:
		MQ::IMQInterface *mp_mqconnection;
		Redis::Connection *mp_redis_connection;
		INetServer *server;
};

class DefaultTask : public OS::Task<TaskThreadData> {
	public:
		DefaultTask() { }
		~DefaultTask() {

        }
};
#endif //_DEFAULTTASK_H