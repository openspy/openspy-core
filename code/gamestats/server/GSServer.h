#ifndef _GPSERVER_H
#define _GPSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
namespace GS {
	class Server : public INetServer {
	public:
		Server();
		~Server();
		void tick();
	private:
	};
}
#endif //_GPSERVER_H