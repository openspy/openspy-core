#ifndef _SMSERVER_H
#define _SMSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>

namespace SM {
	class Server : public INetServer {
	public:
		Server();
		virtual ~Server();
		void tick();
	private:
	};
}
#endif //_SMSERVER_H