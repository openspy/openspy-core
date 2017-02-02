#include "NNPeer.h"
#include "NNServer.h"
#include "NNDriver.h"

namespace NN {

	Server::Server() : INetServer() {
		
	}
	void Server::init() {
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}

}