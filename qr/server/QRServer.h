#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/Net/NetServer.h>
#define MASTER_PORT 27900
namespace QR {
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
#endif //_CHCGAMESERVER_H