#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Task/TaskScheduler.h>
#include <OS/SharedTasks/tasks.h>

namespace UT {
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void init();
		void tick();
		void shutdown();
	private:
	};
}
#endif //_SMSERVER_H