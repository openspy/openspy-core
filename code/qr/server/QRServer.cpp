#include "QRServer.h"
#include "QRDriver.h"
#include <iterator>

#include <tasks/tasks.h>

#include <uv.h>

namespace QR {

	Server::Server() : INetServer() {
		uv_loop_set_data(uv_default_loop(), this);
		//MM::InitTasks();
	}

	Server::~Server() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			delete driver;
			it++;
		}
	}
	void Server::init() {
	}
	void Server::tick() {
	}
	void Server::shutdown() {

	}
	QR::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			if(driver->getListenerSocket()->address == address) {
				return (QR::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}

}