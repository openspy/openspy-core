#include "SBPeer.h"
#include "SBServer.h"
#include "SBDriver.h"
SBServer::SBServer() : INetServer() {
	
}
SBServer::~SBServer() {
}
void SBServer::init() {
	MM::SetupTaskPool(this);
}
void SBServer::tick() {
	NetworkTick();
}
void SBServer::shutdown() {

}
void SBServer::SetTaskPool(OS::TaskPool<MM::MMQueryTask, MM::MMQueryRequest> *pool) {
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