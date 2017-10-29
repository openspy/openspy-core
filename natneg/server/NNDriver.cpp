#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "NNServer.h"
#include <OS/legacy/buffwriter.h>

#include "NNPeer.h"
#include "NNBackend.h"
#include "NNDriver.h"
#include <OS/socketlib/socketlib.h>

namespace NN {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		
		Socket::Init();
		// /NN::Init(this);
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
			printf("Socket error\n");
			//signal error
		}

		m_local_addr.sin_port = Socket::htons(port);
		m_local_addr.sin_addr.s_addr = Socket::htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = Socket::bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}

		makeNonBlocking(m_sd);

		gettimeofday(&m_server_start, NULL);
	}
	Driver::~Driver() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			delete peer;
			it++;
		}
	}
	void Driver::think(bool listener_waiting) {
		TickConnections();

		if (listener_waiting) {
			char recvbuf[MAX_DATA_SIZE + 1];

			struct sockaddr_in si_other;
			socklen_t slen = sizeof(struct sockaddr_in);

			int len = recvfrom(m_sd, (char *)&recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&si_other, &slen);

			Peer *peer = NULL;
			if (len > 0) {
				peer = find_or_create(&si_other);
			}
			else {
				peer = find_client(&si_other);
			}
			if (peer) {
				if (len > 0) {
					recvbuf[len] = 0;
					peer->handle_packet((char *)&recvbuf, len);
				}
				else if (len < 0) {
					peer->SetDelete(true);
				}
			}
		}
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			if (peer->ShouldDelete() && std::find(m_peers_to_delete.begin(), m_peers_to_delete.end(), peer) == m_peers_to_delete.end()) {
				//marked for delection, dec reference and delete when zero
				it = m_connections.erase(it);
				peer->DecRef();
				m_peers_to_delete.push_back(peer);

				m_server->UnregisterSocket(peer);
				continue;
			}
			it++;
		}

		it = m_peers_to_delete.begin();
		while (it != m_peers_to_delete.end()) {
			Peer *p = *it;
			if (p->GetRefCount() == 0) {
				delete p;
				it = m_peers_to_delete.erase(it);
				continue;
			}
			it++;
		}
	}

	Peer *Driver::find_client(struct sockaddr_in *address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in peer_address = peer->getAddress().GetInAddr();
			if (address->sin_port == peer_address.sin_port && address->sin_addr.s_addr == peer_address.sin_addr.s_addr) {
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
			const struct sockaddr_in peer_address = peer->getAddress().GetInAddr();
			if (address->sin_port == peer_address.sin_port && address->sin_addr.s_addr == peer_address.sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		Peer *ret = new Peer(this, address, m_sd);
		m_connections.push_back(ret);
		return ret;
	}


	uint32_t Driver::getBindIP() {
		return htonl(m_local_addr.sin_addr.s_addr);
	}

	void Driver::OnGotCookie(int cookie, int client_idx, OS::Address address) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			if(p->GetCookie() == cookie && p->GetClientIndex() != client_idx) {
				p->OnGotPeerAddress(address);
			}
			it++;
		}
	}

	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}

	const std::vector<INetPeer *> Driver::getPeers() {
		std::vector<INetPeer *> peers;
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer *p = (INetPeer *)*it;
			peers.push_back(p);
			it++;
		}
		return peers;
	}
	const std::vector<int> Driver::getSockets() {
		std::vector<int> sockets;
		sockets.push_back(m_sd);
		return sockets;
	}
	int Driver::getListenerSocket() {
		return m_sd;
	}
	OS::MetricInstance Driver::GetMetrics() {
		OS::MetricInstance peer_metric;
		/*OS::MetricValue arr_value2, value, peers;

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

		while(!m_stats_queue.empty()) {
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
		
		mp_mutex->unlock();*/
		return peer_metric;
	}
}
