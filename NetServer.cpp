#include "NetServer.h"
#include "SelectNetEventManager.h"
INetServer::INetServer() {
	mp_net_event_mgr = new SelectNetEventManager();
}
INetServer::~INetServer() {
	
}
void INetServer::addNetworkDriver(INetDriver *driver) {
	m_net_drivers.push_back(driver);
	mp_net_event_mgr->addNetworkDriver(driver);
}
void INetServer::tick(fd_set *fdset) {
	NetworkTick();
}
void INetServer::NetworkTick() {
	mp_net_event_mgr->run();
}