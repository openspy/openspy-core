#include "CDKeyServer.h"
#include "CDKeyDriver.h"
#include <iterator>
namespace CDKey {

	Server::Server() : INetServer() {
		uv_loop_set_data(uv_default_loop(), this);
	}
	Server::~Server() {
	}
	void Server::tick() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think();
			it++;
		}
		NetworkTick();
	}
	void Server::shutdown() {

	}
	CDKey::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			if(OS::Address(driver->GetAddress()) == address) {
				return (CDKey::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}
}