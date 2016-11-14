#include "QRPeer.h"
#include "QRServer.h"
#include "QRDriver.h"
#include "MMPush.h"
namespace QR {

	Server::Server() {
		
	}
	void Server::init() {
		MM::Init();
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}

}