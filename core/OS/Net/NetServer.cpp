#include "NetServer.h"
#if EVTMGR_USE_SELECT
	#include "SelectNetEventManager.h"
#elif EVTMGR_USE_EPOLL
	#include "EPollNetEventManager.h"
#endif


INetServer::INetServer() {
	#ifdef EVTMGR_USE_SELECT
	SelectNetEventManager *event_mgr;
	event_mgr = new SelectNetEventManager();
	#elif EVTMGR_USE_EPOLL
	EPollNetEventManager *event_mgr;
	event_mgr = new EPollNetEventManager();
	#endif
	mp_net_event_mgr = event_mgr;
	mp_net_io_interface = event_mgr;
}
INetServer::~INetServer() {
	delete mp_net_event_mgr;
}
void INetServer::addNetworkDriver(INetDriver *driver) {
	m_net_drivers.push_back(driver);
	mp_net_event_mgr->addNetworkDriver(driver);
}
void INetServer::tick() {
	NetworkTick();
}
void INetServer::NetworkTick() {
	mp_net_event_mgr->run();
}
void INetServer::RegisterSocket(INetPeer *peer) {
	mp_net_event_mgr->RegisterSocket(peer);
}
void INetServer::UnregisterSocket(INetPeer *peer) {
	mp_net_event_mgr->UnregisterSocket(peer);
}
INetIOInterface *INetServer::getNetIOInterface() {
	return mp_net_io_interface;
}