#include <OS/OpenSpy.h>
#if EVTMGR_USE_EPOLL
	#include "EPollNetEventManager.h"
	#include <stdio.h>
	#include <OS/Net/NetServer.h>
	#include <OS/Net/NetPeer.h>
	#include <algorithm>
	EPollNetEventManager::EPollNetEventManager() : INetEventManager(), BSDNetIOInterface() {
		m_epoll_peers_fd = epoll_create(MAX_EPOLL_EVENTS);
		m_epoll_drivers_fd = epoll_create(MAX_EPOLL_EVENTS);
		m_added_drivers = false;
	}
	EPollNetEventManager::~EPollNetEventManager() {
		close(m_epoll_peers_fd);
		close(m_epoll_drivers_fd);
	}
	void EPollNetEventManager::run() {
		INetDriver *driver = NULL;
		if(!m_added_drivers) {
			setupDrivers();
		}

		struct epoll_event events[MAX_EPOLL_EVENTS];

		int nr_events = epoll_wait(m_epoll_drivers_fd, (epoll_event *)&events, MAX_EPOLL_EVENTS, 15);
		for(int i=0;i<nr_events;i++) {
			INetDriver *driver = (INetDriver *)events[i].data.ptr;
			if(driver == NULL) {
				continue;
			}
			driver->think(true);
		}

		nr_events = epoll_wait(m_epoll_peers_fd, (epoll_event *)&events, MAX_EPOLL_EVENTS, 15);
		for(int i=0;i<nr_events;i++) {
			INetPeer *peer = (INetPeer *)events[i].data.ptr;
			if(peer == NULL) {
				continue;
			}
			peer->think(true);
		}
	}
	void EPollNetEventManager::RegisterSocket(INetPeer *peer, bool notify_driver_only) {
		if(peer->GetDriver()->getListenerSocket() != peer->GetSocket()) {
			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.ptr = peer;
			if(notify_driver_only) {
				//XXX: unhandled! needed for proxy headers
				OS::LogText(OS::ELogLevel_Info, "Warning: unhandled epoll driver notify");
			}
			epoll_ctl(m_epoll_peers_fd, EPOLL_CTL_ADD, peer->GetSocket()->sd, &ev);
		}
	}
	void EPollNetEventManager::UnregisterSocket(INetPeer *peer) {
		epoll_ctl(m_epoll_peers_fd, EPOLL_CTL_DEL, peer->GetSocket()->sd, NULL);
	}

	void EPollNetEventManager::setupDrivers() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while(it != m_net_drivers.end()) {
			INetDriver *driver = *it;

			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.ptr = driver;
			epoll_ctl(m_epoll_drivers_fd, EPOLL_CTL_ADD, driver->getListenerSocket()->sd, &ev);
			it++;
		}
		m_added_drivers = true;
	}
#endif
