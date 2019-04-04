#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/drivers/TCPDriver.h>

#include "SMPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace SM {
	class Peer;

	class Driver : public TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
	protected:
		virtual INetPeer *CreatePeer(INetIOSocket *socket);
	};
}
#endif //_SBDRIVER_H