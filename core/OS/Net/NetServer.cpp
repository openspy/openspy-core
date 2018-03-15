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
	mp_net_event_mgr = event_mgr;
	mp_net_io_interface = event_mgr;
	#elif EVTMGR_USE_EPOLL
	mp_net_event_mgr = new EPollNetEventManager();
	#endif
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
void INetServer::flagExit() {
	mp_net_event_mgr->flagExit();	
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