#include <OS/OpenSpy.h>
#include "GSPeer.h"
#include "GSServer.h"
#include "GSDriver.h"

namespace GS {
	Server::Server() : INetServer() {
		uv_loop_set_data(uv_default_loop(), this);
	}
	Server::~Server() {
	}

	void Server::tick() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think(false);
			it++;
		}
		NetworkTick();
	}
}