#include "QRPeer.h"
#include "QRServer.h"
#include "QRDriver.h"
#include "MMPush.h"

namespace QR {

	Server::Server() : INetServer() {
		
	}

	Server::~Server() {
		
	}
	void Server::init() {
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}

}