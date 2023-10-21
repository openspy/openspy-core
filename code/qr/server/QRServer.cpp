#include "QRServer.h"
#include "QRDriver.h"
#include <iterator>

namespace QR {

	Server::Server() : INetServer() {
		uv_loop_set_data(uv_default_loop(), this);
	}

	Server::~Server() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			delete driver;
			it++;
		}
	}
	void Server::tick() {
	}
	QR::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			QR::Driver *driver = (QR::Driver *)*it;
			if(OS::Address(driver->GetAddress()) == address) {
				return (QR::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}

}