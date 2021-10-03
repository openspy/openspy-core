#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>

#include <tasks/tasks.h>
#define MASTER_PORT 27900
namespace MM {
	class MMPushTask;
	class MMPushRequest;
}
namespace QR {
	class Peer;
	class Server : public INetServer {
	public:
		Server();
		~Server();
		void init();
		void tick();
		void shutdown();
		TaskScheduler<MM::MMPushRequest, TaskThreadData> *getScheduler() { return mp_task_scheduler; };
		Driver *findDriverByAddress(OS::Address address);
	private:
		TaskScheduler<MM::MMPushRequest, TaskThreadData> *mp_task_scheduler;
	};
}
#endif //_CHCGAMESERVER_H