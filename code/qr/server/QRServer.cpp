#include "QRPeer.h"
#include "QRServer.h"
#include "QRDriver.h"
#include <iterator>

#include <tasks/tasks.h>
namespace QR {

	Server::Server() : INetServer() {
	}

	Server::~Server() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			delete driver;
			it++;
		}
		delete mp_task_scheduler;
	}
	void Server::init() {
		mp_task_scheduler = MM::InitTasks(this);
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
	Peer *Server::find_client(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			QR::Driver *driver = (QR::Driver *)*it;
			QR::Peer *peer = driver->find_client(address);
			if (peer) {
				return peer;
			}
			it++;
		}
		return NULL;
	}
}