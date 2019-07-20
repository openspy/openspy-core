#include "NNServer.h"
#include "NNDriver.h"
#include <tasks/tasks.h>
#include <iterator>
namespace NN {

	Server::Server() : INetServer() {
		mp_task_scheduler = NULL;
	}
	void Server::init() {
		mp_task_scheduler = InitTasks(this);
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
	NN::Driver *Server::findDriverByAddress(OS::Address address) {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			if(driver->getListenerSocket()->address == address) {
				return (NN::Driver *)driver;
			}
			it++;
		}
		return NULL;
	}
}