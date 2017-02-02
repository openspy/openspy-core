#include "GPPeer.h"
#include "GPServer.h"
#include "GPDriver.h"

namespace GP {
	Server::Server() : INetServer(){
	}
	void Server::init() {
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}

}