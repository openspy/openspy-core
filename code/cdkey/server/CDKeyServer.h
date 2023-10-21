#ifndef _CDKEYSERVER_H
#define _CDKEYSERVER_H
#include <stdint.h>
#include <OS/OpenSpy.h>
#include <OS/Net/NetServer.h>

namespace CDKey {
	class Driver;
	class Server : public INetServer {
		public:
			Server();
			virtual ~Server();
			void tick();
			void shutdown();
			CDKey::Driver *findDriverByAddress(OS::Address address);
		private:
	};
}
#endif //_CHCGAMESERVER_H