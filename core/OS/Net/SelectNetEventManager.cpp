#include <OS/OpenSpy.h>
#include "SelectNetEventManager.h"
#include <stdio.h>
#include "NetPeer.h"
#include <OS/socketlib/socketlib.h>
SelectNetEventManager::SelectNetEventManager() {
	m_exit_flag = false;
}
SelectNetEventManager::~SelectNetEventManager() {
	m_exit_flag = true;
}
void SelectNetEventManager::run() {
	
	struct timeval timeout;

    memset(&timeout,0,sizeof(struct timeval));
    timeout.tv_sec = 1;

    int hsock = setup_fdset();
    if(Socket::select(hsock + 1, &m_fdset, NULL, NULL, &timeout) < 0) {
    	//return;
    }
    if(m_exit_flag) {
    	return;
    }

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		driver->think(FD_ISSET(driver->getListenerSocket(), &m_fdset));

		std::vector<INetPeer *> net_peers;
		std::vector<INetPeer *>::iterator it2 = net_peers.begin();
		while (it2 != net_peers.end()) {
			INetPeer *peer = *it2;
			int sd = peer->GetSocket();
			peer->think(FD_ISSET(sd, &m_fdset));
			it2++;
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

		std::vector<int> sockets = driver->getSockets();
		std::vector<int>::iterator it2 = sockets.begin();
		int sd;
		while (it2 != sockets.end()) {
			sd = *it2;
			if (sd >= hsock) {
				hsock = sd + 1;
			}

			FD_SET(sd, &m_fdset);
			it2++;
		}
		//add listening socket/clients to fd

		it++;
	}
	return hsock;
}
