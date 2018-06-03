#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#define SM_SERVER_PORT 29901
namespace SM {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
	private:
	};
}
#endif //_SMSERVER_H