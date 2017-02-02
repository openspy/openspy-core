#include "SMPeer.h"
#include "SMServer.h"
#include "SMDriver.h"
namespace SM {
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