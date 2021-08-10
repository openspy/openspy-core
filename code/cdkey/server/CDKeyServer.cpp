#include "CDKeyServer.h"
#include "CDKeyDriver.h"
#include <iterator>
namespace CDKey {

	Server::Server() : INetServer() {
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
	CDKey::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			if(driver->getListenerSocket()->address == address) {
				return (CDKey::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}
}