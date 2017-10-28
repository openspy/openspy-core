#include <OS/OpenSpy.h>

#if EVTMGR_USE_SELECT

#include "SelectNetEventManager.h"
#include <stdio.h>
#include "NetPeer.h"
#include <OS/socketlib/socketlib.h>
SelectNetEventManager::SelectNetEventManager() {
	m_exit_flag = false;
	m_dirty_fdset = true;
}
SelectNetEventManager::~SelectNetEventManager() {
	m_exit_flag = true;
}
void SelectNetEventManager::run() {
	
	struct timeval timeout;

    memset(&timeout,0,sizeof(struct timeval));
    timeout.tv_usec = SELECT_TIMEOUT;

	int hsock = setup_fdset();

    if(Socket::select(hsock, &m_fdset, NULL, NULL, &timeout) < 0) {
    	//return;
    }
    if(m_exit_flag) {
    	return;
    }

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if(FD_ISSET(driver->getListenerSocket(), &m_fdset)) {
			driver->think(true);
		}

		std::vector<INetPeer *> net_peers = driver->getPeers();
		std::vector<INetPeer *>::iterator it2 = net_peers.begin();
		while (it2 != net_peers.end()) {
			INetPeer *peer = *it2;
			int sd = peer->GetSocket();
			if(sd != driver->getListenerSocket())
				peer->think(FD_ISSET(sd, &m_fdset));
			it2++;
		}
		it++;
	}
}
int SelectNetEventManager::setup_fdset() {
	
	FD_ZERO(&m_fdset);
	int hsock = -1;
	if (m_dirty_fdset) {
		m_cached_sockets.clear();
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;

			std::vector<int> sockets = driver->getSockets();
			std::vector<int>::iterator it2 = sockets.begin();
			int sd = driver->getListenerSocket();
			FD_SET(sd, &m_fdset);
			m_cached_sockets.push_back(sd);
			if (sd > hsock) {
				hsock = sd;
			}
			while (it2 != sockets.end()) {
				sd = *it2;
				if (sd > hsock) {
					hsock = sd;
				}
				if (sd != driver->getListenerSocket()) {
					FD_SET(sd, &m_fdset);
					m_cached_sockets.push_back(sd);
				}
				it2++;
			}
			it++;
		}
		m_hsock = hsock + 1;
		m_dirty_fdset = false;
		return m_hsock;
	}
	else {
		std::vector<int>::iterator it = m_cached_sockets.begin();
		while (it != m_cached_sockets.end()) {
			int sd = *it;
			FD_SET(sd, &m_fdset);
			it++;
		}
		return m_hsock;
	}
}
void SelectNetEventManager::RegisterSocket(INetPeer *peer) {
	m_dirty_fdset = true;
}
void SelectNetEventManager::UnregisterSocket(INetPeer *peer) {
	m_dirty_fdset = true;
}
#endif