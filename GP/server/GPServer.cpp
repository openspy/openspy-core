#include "GPPeer.h"
#include "GPServer.h"
#include "GPDriver.h"

namespace GP {
	Server::Server() : INetServer(){
	}
	void Server::init() {
		GPBackend::SetupTaskPool(this);
	}
	void Server::tick() {
		NetworkTick();
	}
	void Server::shutdown() {

	}
	void Server::SetTaskPool(OS::TaskPool<GPBackend::GPBackendRedisTask, GPBackend::GPBackendRedisRequest> *pool) {
		const std::vector<GPBackend::GPBackendRedisTask *> task_list = pool->getTasks();
		std::vector<GPBackend::GPBackendRedisTask *>::const_iterator it = task_list.begin();
		while (it != task_list.end()) {
			GPBackend::GPBackendRedisTask *task = *it;
			std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
			while (it2 != m_net_drivers.end()) {
				GP::Driver *driver = (GP::Driver *)*it2;
				task->AddDriver(driver);
				it2++;
			}
			it++;
		}
	}

}