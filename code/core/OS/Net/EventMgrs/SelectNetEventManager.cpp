#include <OS/OpenSpy.h>

#if EVTMGR_USE_SELECT

#include "SelectNetEventManager.h"
#include <stdio.h>
#include <OS/Net/NetPeer.h>
SelectNetEventManager::SelectNetEventManager() : BSDNetIOInterface(), INetEventManager() {
	mp_mutex = OS::CreateMutex();
}
SelectNetEventManager::~SelectNetEventManager() {
	delete mp_mutex;
}
void SelectNetEventManager::run() {

	struct timeval timeout;

	memset(&timeout, 0, sizeof(struct timeval));
	timeout.tv_sec = SELECT_TIMEOUT;

	socktype_t hsock = setup_fdset();

	if (select((int)hsock, &m_fdset, NULL, NULL, &timeout) < 0) {
		//return;
	}

	mp_mutex->lock();

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if (FD_ISSET(driver->getListenerSocket()->sd, &m_fdset))
			driver->think(true);

		INetPeer* peer = driver->GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				INetIOSocket* sd = peer->GetSocket();
				if (sd != peer->GetDriver()->getListenerSocket() && FD_ISSET(sd->sd, &m_fdset))
					peer->think(true);
			} while ((peer = peer->GetNext()) != NULL);
		}
		it++;
	}

	flushSendQueue();

	mp_mutex->unlock();
}
socktype_t SelectNetEventManager::setup_fdset() {
	socktype_t hsock = 0;
	mp_mutex->lock();
	FD_ZERO(&m_fdset);

	std::vector<INetDriver*>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver* driver = *it;
		INetIOSocket* sd = driver->getListenerSocket();
		FD_SET(sd->sd, &m_fdset);
		if (sd->sd > hsock) {
			hsock = sd->sd;
		}

		INetPeer* peer = driver->GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				INetIOSocket* sd = peer->GetSocket();
				FD_SET(sd->sd, &m_fdset);
				if (sd->sd > hsock) {
					hsock = sd->sd;
				}
			} while ((peer = peer->GetNext()) != NULL);
		}
		it++;
	}

	m_hsock = hsock + 1;
	mp_mutex->unlock();
	return m_hsock;
}
void SelectNetEventManager::RegisterSocket(INetPeer *peer, bool notify_driver_only) {
}
void SelectNetEventManager::UnregisterSocket(INetPeer *peer) {
    mp_mutex->lock();
    FD_CLR(peer->GetSocket()->sd, &m_fdset);
    mp_mutex->unlock(); //overkill?
}

#endif
