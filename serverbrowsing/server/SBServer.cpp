#include "SBPeer.h"
#include "SBServer.h"
#include "SBDriver.h"
#include <OS/OpenSpy.h>
#include <tasks/tasks.h>
SBServer::SBServer() : INetServer() {
}
SBServer::~SBServer() {
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		delete *it;
		it++;
	}
}
void SBServer::init() {
	mp_task_scheduler = MM::InitTasks(this);
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