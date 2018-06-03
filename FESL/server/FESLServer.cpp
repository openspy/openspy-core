#include "FESLPeer.h"
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLBackend.h"

namespace FESL {
	Server::Server() : INetServer(){
	}
	void Server::init() {
		FESLBackend::SetupFESLBackend(this);
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
		FESLBackend::ShutdownFESLBackend();
	}
	void Server::OnUserAuth(OS::Address remote_address, int userid, int profileid) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			FESL::Driver *driver = (FESL::Driver *) *it;
			driver->OnUserAuth(remote_address, userid, profileid);
			it++;
		}
	}
}