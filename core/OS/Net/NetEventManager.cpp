#include "NetEventManager.h"
INetEventManager::INetEventManager() {
	m_exit_flag = false;

}
INetEventManager::~INetEventManager() {
	m_exit_flag = true;
}
void INetEventManager::addNetworkDriver(INetDriver *driver) {
	m_net_drivers.push_back(driver);
}
void INetEventManager::flagExit() {
	m_exit_flag = true;
}