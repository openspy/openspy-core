#include "SelectNetEventManager.h"
#include <stdio.h>
#include <OS/socketlib/socketlib.h>
SelectNetEventManager::SelectNetEventManager() {

}
SelectNetEventManager::~SelectNetEventManager() {

}
void SelectNetEventManager::run() {
	
	struct timeval timeout;

    memset(&timeout,0,sizeof(struct timeval));
    timeout.tv_usec = 16000;

    int hsock = setup_fdset();
    if(Socket::select(hsock + 1, &m_fdset, NULL, NULL, &timeout) < 0) {
    	return;
    }

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if(FD_ISSET(driver->getListenerSocket(), &m_fdset)) {
			driver->tick(&m_fdset);
		} else {
			driver->think(&m_fdset);
		}
		it++;
	}
}
int SelectNetEventManager::setup_fdset() {
	FD_ZERO(&m_fdset);
	int hsock = -1;
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;

		//add listening socket/clients to fd
		int ret = driver->setup_fdset(&m_fdset);
		if(ret >= hsock)
			hsock = ret+1;
		it++;
	}
	return hsock;
}