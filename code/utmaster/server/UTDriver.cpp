#include <stdio.h>
#include <stdlib.h>
#include "UTServer.h"
#include "UTPeer.h"
#include "UTDriver.h"

namespace UT {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		return new Peer(this, socket);
	}
}