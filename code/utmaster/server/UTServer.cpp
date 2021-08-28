#include <OS/SharedTasks/tasks.h>
#include "UTPeer.h"
#include "UTServer.h"
#include "UTDriver.h"

#define MESSAGE_CHALLENGE 0x0b

namespace UT {
	Server::Server() : INetServer(){
	}
	Server::~Server() {
	}
	void Server::init() {
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
	void Server::shutdown() {

	}
}