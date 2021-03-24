#include <OS/OpenSpy.h>
#if EVTMGR_USE_EPOLL
	#include "EPollNetEventManager.h"
	#include <stdio.h>
	#include <OS/Net/NetServer.h>
	#include <OS/Net/NetPeer.h>
	#include <algorithm>

	EPollNetEventManager::EPollNetEventManager() : INetEventManager(), BSDNetIOInterface() {
		m_epollfd = epoll_create(MAX_EPOLL_EVENTS);
		m_added_drivers = false;
		mp_data_info_head = new OS::LinkedListHead<EPollDataInfo *>();
	}
	EPollNetEventManager::~EPollNetEventManager() {
		UnregisterSocketIteratorState state;
		state.event_manager = this;
		state.unregisterTarget = NULL;

		OS::LinkedListIterator<EPollDataInfo*, UnregisterSocketIteratorState*> iterator(mp_data_info_head);
		iterator.Iterate(LLIterator_DeleteAll, &state);
		close(m_epollfd);
	}
	void EPollNetEventManager::run() {
		INetDriver *driver = NULL;
		if(!m_added_drivers) {
			setupDrivers();
		}
		int nr_events = epoll_wait (m_epollfd, (epoll_event *)&m_events, MAX_EPOLL_EVENTS, EPOLL_TIMEOUT);
		for(int i=0;i<nr_events;i++) {
			EPollDataInfo *data = (EPollDataInfo *)m_events[i].data.ptr;
			if(data->is_peer) {
				INetPeer *peer = (INetPeer *)data->ptr;
				if(data->is_peer_notify_driver) {
					driver = peer->GetDriver();
					driver->OnPeerMessage(peer);
				} else {
					peer->think(true);
				}
			} else {
				driver = (INetDriver *)data->ptr;
				driver->think(true);
			}
		}


		//force TCP accept, incase of high connection load, will not block due to non-blocking sockets
		if(nr_events == 0) {
			std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
			while(it != m_net_drivers.end()) {
				INetDriver *driver = *it;
				driver->think(true);
				it++;
			}
		}
		flushSendQueue();
	}
	void EPollNetEventManager::RegisterSocket(INetPeer *peer, bool notify_driver_only) {
		if(peer->GetDriver()->getListenerSocket() != peer->GetSocket()) {
			EPollDataInfo *data_info = new EPollDataInfo();

			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
			ev.data.ptr = data_info;

			data_info->ptr = peer;
			data_info->is_peer = true;
			data_info->is_peer_notify_driver = notify_driver_only;

			mp_data_info_head->AddToList(data_info);

			epoll_ctl(m_epollfd, EPOLL_CTL_ADD, peer->GetSocket()->sd, &ev);
		}
	}
	bool EPollNetEventManager::LLIterator_UnregisterSocket(EPollDataInfo* data_info, UnregisterSocketIteratorState* state) {
		if(data_info->is_peer && data_info->ptr == state->unregisterTarget) {
			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
			ev.data.ptr = state->unregisterTarget;
			epoll_ctl(state->event_manager->m_epollfd, EPOLL_CTL_DEL, state->unregisterTarget->GetSocket()->sd, &ev);
			
			
			state->event_manager->mp_data_info_head->RemoveFromList(data_info);
			delete data_info;
			return false;
		}
		return true;
	}
	void EPollNetEventManager::UnregisterSocket(INetPeer *peer) {
		if(peer->GetDriver()->getListenerSocket() != peer->GetSocket()) {

			UnregisterSocketIteratorState state;
			state.event_manager = this;
			state.unregisterTarget = peer;

			OS::LinkedListIterator<EPollDataInfo*, UnregisterSocketIteratorState*> iterator(mp_data_info_head);
    		iterator.Iterate(LLIterator_UnregisterSocket, &state);
		}
	}
	bool EPollNetEventManager::LLIterator_DeleteAll(EPollDataInfo* data_info, UnregisterSocketIteratorState* state) {
			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
			ev.data.ptr = data_info->ptr;
		if(data_info->is_peer) {
			INetPeer *peer = (INetPeer *)data_info->ptr;
			epoll_ctl(state->event_manager->m_epollfd, EPOLL_CTL_DEL, peer->GetSocket()->sd, &ev);
		} else {
			INetDriver *driver = (INetDriver *)data_info->ptr;
			epoll_ctl(state->event_manager->m_epollfd, EPOLL_CTL_DEL, driver->getListenerSocket()->sd, &ev);
		}
		
		state->event_manager->mp_data_info_head->RemoveFromList(data_info);
		delete data_info;
		return true;
	}

	void EPollNetEventManager::setupDrivers() {
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while(it != m_net_drivers.end()) {
			INetDriver *driver = *it;

			EPollDataInfo *data_info = new EPollDataInfo();
			data_info->ptr = driver;
			data_info->is_peer = false;

			mp_data_info_head->AddToList(data_info);			

			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
			ev.data.ptr = data_info;

			epoll_ctl(m_epollfd, EPOLL_CTL_ADD, driver->getListenerSocket()->sd, &ev);
			it++;
		}
		m_added_drivers = true;
	}
#endif
