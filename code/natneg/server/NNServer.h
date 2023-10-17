#ifndef _QRSERVER_H
#define _QRSERVER_H
#include <stdint.h>
#include <OS/OpenSpy.h>
#include <OS/Net/NetServer.h>

#include <tasks/tasks.h>

#include "structs.h"

#define NATNEG_PORT 27901
namespace NN {
	class Driver;
	class Server : public INetServer {
		public:
			Server();
			virtual ~Server();
			void init();
			void tick();
			void shutdown();
			NN::Driver *findDriverByAddress(OS::Address address);
		private:
	};
}
#endif //_CHCGAMESERVER_H