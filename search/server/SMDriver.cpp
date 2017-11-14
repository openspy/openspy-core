#include "SMDriver.h"
#include <stdio.h>
#include <stdlib.h>

#include "SMServer.h"
#include "SMPeer.h"

namespace SM {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		m_local_addr.sin_port = htons(port);
		m_local_addr.sin_addr.s_addr = htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}
		if (listen(m_sd, SOMAXCONN)
			< 0) {
			//signal error
		}

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(Driver::TaskThread, this, true);
	}
	Driver::~Driver() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			delete peer;
			it++;
		}
		delete mp_mutex;
		delete mp_thread;
	}
	void *Driver::TaskThread(OS::CThread *thread) {
		Driver *driver = (Driver *)thread->getParams();
		for (;;) {
			driver->mp_mutex->lock();
			std::vector<Peer *>::iterator it = driver->m_connections.begin();
			while (it != driver->m_connections.end()) {
				Peer *peer = *it;
				if (peer->ShouldDelete() && std::find(driver->m_peers_to_delete.begin(), driver->m_peers_to_delete.end(), peer) == driver->m_peers_to_delete.end()) {
					//marked for delection, dec reference and delete when zero
					it = driver->m_connections.erase(it);
					peer->DecRef();

					driver->m_server->UnregisterSocket(peer);

					driver->m_stats_queue.push(peer->GetPeerStats());
					driver->m_peers_to_delete.push_back(peer);
					continue;
				}
				it++;
			}

			it = driver->m_peers_to_delete.begin();
			while (it != driver->m_peers_to_delete.end()) {
				SM::Peer *p = *it;
				if (p->GetRefCount() == 0) {
					delete p;
					it = driver->m_peers_to_delete.erase(it);
					continue;
				}
				it++;
			}

			driver->TickConnections();
			driver->mp_mutex->unlock();
			OS::Sleep(DRIVER_THREAD_TIME);
		}
	}
	void Driver::think(bool listener_waiting) {
		if (listener_waiting) {
			while (true) {
				socklen_t psz = sizeof(struct sockaddr_in);
				struct sockaddr_in peer;
				int sda = accept(m_sd, (struct sockaddr *)&peer, &psz);
				if (sda <= 0) return;
				Peer *mp_peer = new Peer(this, &peer, sda);

				makeNonBlocking(mp_peer);

				mp_mutex->lock();
				m_connections.push_back(mp_peer);
				m_server->RegisterSocket(mp_peer);
				mp_mutex->unlock();
				//mp_peer->think(true);
			}
		}
	}

	const std::vector<INetPeer *> Driver::getPeers(bool inc_ref) {
		mp_mutex->lock();
		std::vector<INetPeer *> peers;
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer * p = (INetPeer *)*it;
			peers.push_back(p);
			if (inc_ref)
				p->IncRef();
			it++;
		}
		mp_mutex->unlock();
		return peers;
	}

	int Driver::GetNumConnections() {
		return m_connections.size();
	}

	const std::vector<int> Driver::getSockets() {
		std::vector<int> sockets;
		mp_mutex->lock();
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			sockets.push_back(p->GetSocket());
			it++;
		}
		mp_mutex->unlock();
		return sockets;
	}

	Peer *Driver::find_client(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		return NULL;
	}
	Peer *Driver::find_or_create(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		Peer *ret = new Peer(this, address, m_sd);
		m_connections.push_back(ret);
		return ret;
	}
	bool Driver::HasPeer(Peer *peer) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			if((*it) == peer)
				return true;
			it++;
		}
		return false;
	}

	int Driver::getListenerSocket() {
		return m_sd;
	}

	uint32_t Driver::getBindIP() {
		return htonl(m_local_addr.sin_addr.s_addr);
	}
	uint32_t Driver::getDeltaTime() {
		struct timeval now;
		gettimeofday(&now, NULL);
		uint32_t t = (now.tv_usec / 1000.0) - (m_server_start.tv_usec / 1000.0);
		return t;
	}
	
	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}
	OS::MetricInstance Driver::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue arr_value2, value, peers;

		mp_mutex->lock();

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer * peer = (INetPeer *)*it;
			OS::Address address = *peer->getAddress();
			value = peer->GetMetrics().value;

			value.key = address.ToString(false);

			peers.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, value));
			it++;
		}

		while (!m_stats_queue.empty()) {
			PeerStats stats = m_stats_queue.front();
			m_stats_queue.pop();
			peers.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, Peer::GetMetricItemFromStats(stats)));
		}

		peers.key = "peers";
		arr_value2.type = OS::MetricType_Array;
		peers.type = OS::MetricType_Array;
		arr_value2.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, peers));


		peer_metric.key = OS::Address(m_local_addr).ToString(false);
		arr_value2.key = peer_metric.key;
		peer_metric.value = arr_value2;

		mp_mutex->unlock();
		return peer_metric;
	}
}