#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#include <OS/Net/IOIfaces/SSLIOInterface.h>
#define FESL_SERVER_PORT 18300

namespace FESL {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();
		void OnUserAuth(OS::Address remote_address, int userid, int profileid);
	private:
	};
}
#endif //_SMSERVER_H