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
		~Server();
		void init();
		void tick();
		void shutdown();
	private:
	};
}
#endif //_GPSERVER_H