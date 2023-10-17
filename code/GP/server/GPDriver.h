#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/drivers/TCPDriver.h>

#include "GPPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <OS/GPShared.h>

#define GP_PING_TIME (60)

namespace GP {
	class Peer;
	class Driver;
	class Driver : public OS::TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port);
		Peer *FindPeerByProfileID(int profileid);

		void InformStatusUpdate(int from_profileid, GPShared::GPStatus status);
		
	protected:
		virtual INetPeer *CreatePeer(uv_tcp_t *socket);
	};
}
#endif //_SBDRIVER_H