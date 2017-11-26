#include <OS/OpenSpy.h>

#if EVTMGR_USE_SELECT

#include "SelectNetEventManager.h"
#include <stdio.h>
#include "NetPeer.h"
SelectNetEventManager::SelectNetEventManager() {
	m_exit_flag = false;
	m_dirty_fdset = true;
	mp_mutex = OS::CreateMutex();
}
SelectNetEventManager::~SelectNetEventManager() {
	m_exit_flag = true;
	delete mp_mutex;
}
void SelectNetEventManager::run() {

	struct timeval timeout;

	memset(&timeout, 0, sizeof(struct timeval));
	timeout.tv_sec = SELECT_TIMEOUT;

	int hsock = setup_fdset();

	if (select(hsock, &m_fdset, NULL, NULL, &timeout) < 0) {
		//return;
	}
	if (m_exit_flag) {
		return;
	}
	mp_mutex->lock();

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if (FD_ISSET(driver->getListenerSocket(), &m_fdset))
			driver->think(true);
		it++;
	}

	std::vector<INetPeer *>::iterator it2 = m_peers.begin();
	while (it2 != m_peers.end()) {
		INetPeer *peer = *it2;
		int sd = peer->GetSocket();
		if (sd != peer->GetDriver()->getListenerSocket() && FD_ISSET(sd, &m_fdset))
			peer->think(true);
		it2++;
	}
	mp_mutex->unlock();
}
int SelectNetEventManager::setup_fdset() {
	int hsock = -1;
	mp_mutex->lock();
	FD_ZERO(&m_fdset);
	if (m_dirty_fdset) {
		m_cached_sockets.clear();
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			int sd = driver->getListenerSocket();
			m_cached_sockets.push_back(sd);
			FD_SET(sd, &m_fdset);
			it++;
		}

		std::vector<INetPeer*>::iterator it2 = m_peers.begin();
		while (it2 != m_peers.end()) {
			INetPeer *peer = *it2;
			int sd = peer->GetSocket();
			m_cached_sockets.push_back(sd);
			FD_SET(sd, &m_fdset);
			it2++;
		}
		m_hsock = hsock + 1;
		m_dirty_fdset = false;
		mp_mutex->unlock();
		return m_hsock;
	}
	else {
		std::vector<int>::iterator it = m_cached_sockets.begin();
		while (it != m_cached_sockets.end()) {
			int sd = *it;
			FD_SET(sd, &m_fdset);
			it++;
		}
		mp_mutex->unlock();
		return m_hsock;
	}
}
void SelectNetEventManager::RegisterSocket(INetPeer *peer) {
	mp_mutex->lock();
	m_dirty_fdset = true;
	m_peers.push_back(peer);
	mp_mutex->unlock();
}
void SelectNetEventManager::UnregisterSocket(INetPeer *peer) {
	mp_mutex->lock();
	std::vector<INetPeer *>::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
	if (it != m_peers.end()) {
		m_peers.erase(it);
	}

	m_dirty_fdset = true;
	mp_mutex->unlock();
}
#endif