#include "NetServer.h"

INetServer::INetServer() {
}
INetServer::~INetServer() {
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		delete *it;
		it++;
	}
}
void INetServer::addNetworkDriver(INetDriver *driver) {
	m_net_drivers.push_back(driver);
}
void INetServer::tick() {
}
void INetServer::NetworkTick() {
}
void INetServer::RegisterSocket(INetPeer *peer, bool notify_driver_only) {
}
void INetServer::UnregisterSocket(INetPeer *peer) {
}