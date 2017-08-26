#include <OS/OpenSpy.h>
#if EVTMGR_USE_EPOLL
	#include "EPollNetEventManager.h"
	#include <stdio.h>
	#include "NetPeer.h"
	#include <OS/socketlib/socketlib.h>


	EPollNetEventManager::EPollNetEventManager() {
		m_exit_flag = false;
		
		m_epollfd = epoll_create(MAX_EPOLL_EVENTS);
		
		m_added_drivers = false;
	}
	EPollNetEventManager::~EPollNetEventManager() {
		m_exit_flag = true;
		
		close(m_epollfd);
	}
	void EPollNetEventManager::run() {
		if(!m_added_drivers) {
			setupDrivers();
		}
		int nr_events = epoll_wait (m_epollfd, &m_events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);
		
		for(int i=0;i<nr_events;i++) {
			EPollDataInfo *data = (EPollDataInfo *)m_events[i].ptr;
			if(data->is_peer) {
				INetPeer *peer = (INetPeer *)data->ptr;
				peer->think(true);
			} else {
				INetDriver *driver = (INetDriver *)data->ptr;
				driver->think(true);
			}
		}
		
		//force peer think false for ping logic, etc
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while(it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			driver->think(false);
			it++;
		}
	}
	void EPollNetEventManager::RegisterSocket(INetPeer *peer) {
		EPollDataInfo *data_info = (EPollDataInfo *)malloc(sizeof(EPollDataInfo));
		
		struct epoll_event ev;
		ev.events = 0;
		ev.data.ptr = data_info;

		data_info->ptr = peer;
		data_info->is_peer = true;
		
		m_datainfo_map[peer] = data_info;
		
		epoll_ctl(m_epollfd, EPOLL_CTL_ADD, peer->getSocket(), &ev);
	}
	void EPollNetEventManager::UnregisterSocket(INetPeer *peer) {
		if(m_datainfo_map.find(peer) != m_datainfo_map.end()) {
			struct epoll_event ev;
			ev.event = 0;
			ev.data.ptr = peer;
			epoll_ctl(m_epollfd, EPOLL_CTL_DEL, peer->getSocket(), &ev);
			free((void *)m_datainfo_map[peer]);
		}
		
	}

	void EPollNetEventManager::setupDrivers() {
		EPollDataInfo *data_info;
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while(it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			
			EPollDataInfo *data_info = (EPollDataInfo *)malloc(sizeof(EPollDataInfo));
			
			data_info->ptr = driver;
			data_info->is_peer = false;
			
			struct epoll_event ev;
			ev.events = 0;
			ev.data.ptr = data_info;	
			
			m_datainfo_map[driver] = data_info;
			
			epoll_ctl(m_epollfd, EPOLL_CTL_ADD, driver->getListenerSocket(), &ev);
			it++;
		}
		m_added_drivers = true;
	}
#endif