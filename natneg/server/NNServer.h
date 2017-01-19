#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#define NATNEG_PORT 27901
namespace NN {
	class Server : public INetServer {
	public:
		Server();
		void init();
		void tick();
		void shutdown();

	private:
		
	};
}
#endif //_CHCGAMESERVER_H