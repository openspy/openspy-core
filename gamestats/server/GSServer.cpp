#include <OS/OpenSpy.h>
#include <OS/TaskPool.h>
#include "GSPeer.h"
#include "GSServer.h"
#include "GSDriver.h"

namespace GS {
	Server::Server() : INetServer(){
	}
	void Server::init() {
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}
	void Server::SetTaskPool(OS::TaskPool<GSBackend::PersistBackendTask, GSBackend::PersistBackendRequest> *pool) {
		const std::vector<GSBackend::PersistBackendTask *> task_list = pool->getTasks();
		std::vector<GSBackend::PersistBackendTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			GSBackend::PersistBackendTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				GS::Driver *driver = (GS::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
	}
}