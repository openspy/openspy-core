#include "NetEventManager.h"
INetEventManager::INetEventManager() {

}
INetEventManager::~INetEventManager() {

}
void INetEventManager::addNetworkDriver(INetDriver *driver) {
	m_net_drivers.push_back(driver);
}