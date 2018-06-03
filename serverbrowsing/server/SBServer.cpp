#include "SBPeer.h"
#include "SBServer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>

SBServer::SBServer() : INetServer() {
}
SBServer::~SBServer() {
	MM::ShutdownTaskPool();
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		delete *it;
		it++;
	}
}
void SBServer::init() {
	MM::SetupTaskPool(this);
}
void SBServer::tick() {
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		driver->think(false);
		it++;
	}
	NetworkTick();
}
void SBServer::SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool) {
	mp_task_pool = pool;
	const std::vector<MM::MMQueryTask *> task_list = pool->getTasks();
	std::vector<MM::MMQueryTask *>::const_iterator it = task_list.begin();
	while (it != task_list.end()) {
		MM::MMQueryTask *task = *it;
		std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
		while (it2 != m_net_drivers.end()) {
			SB::Driver *driver = (SB::Driver *)*it2;
			task->AddDriver(driver);
			it2++;
		}
		it++;
	}
}

void SBServer::debug_dump() {
	std::vector<INetDriver *>::iterator it2 = m_net_drivers.begin();
	while (it2 != m_net_drivers.end()) {
		SB::Driver *driver = (SB::Driver *)*it2;
		driver->debug_dump();
		it2++;
	}

	printf("Task Pool: \n");
	std::vector<MM::MMQueryTask *> task_list = mp_task_pool->getTasks();
	std::vector<MM::MMQueryTask *>::const_iterator it = task_list.begin();
	while (it != task_list.end()) {
		MM::MMQueryTask *task = *it;
		task->debug_dump();
		it++;
	}
}