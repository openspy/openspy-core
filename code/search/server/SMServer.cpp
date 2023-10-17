#include "SMPeer.h"
#include "SMServer.h"
#include "SMDriver.h"


namespace SM {
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