#ifndef _GPSERVER_H
#define _GPSERVER_H
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
#endif //_GPSERVER_H