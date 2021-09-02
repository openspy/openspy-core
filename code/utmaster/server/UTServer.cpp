#include <OS/SharedTasks/tasks.h>
#include "UTPeer.h"
#include "UTServer.h"
#include "UTDriver.h"

#include <tasks/tasks.h>
namespace UT {
	Server::Server() : INetServer(){
	}
	Server::~Server() {
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
}