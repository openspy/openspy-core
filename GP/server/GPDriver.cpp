#include "GPDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "GPServer.h"
#include "GPPeer.h"
#include <OS/socketlib/socketlib.h>

#include <OS/GPShared.h>

namespace GP {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (Socket::setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		m_local_addr.sin_port = Socket::htons(port);
		m_local_addr.sin_addr.s_addr = Socket::htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = Socket::bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}
		if (Socket::listen(m_sd, SOMAXCONN)
			< 0) {
			//signal error
		}

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
	void Driver::think(bool listen_waiting) {
		if (listen_waiting) {
			socklen_t psz = sizeof(struct sockaddr_in);
			struct sockaddr_in address;
			int sda = Socket::accept(m_sd, (struct sockaddr *)&address, &psz);
			if (sda <= 0) return;
			Peer *peer = new Peer(this, &address, sda);

			makeNonBlocking(sda);
			m_connections.push_back(peer);
			m_server->RegisterSocket(peer);
		}
		else {
			std::vector<Peer *>::iterator it = m_connections.begin();
			while (it != m_connections.end()) {
				Peer *peer = *it;
				if (peer->ShouldDelete() && std::find(m_peers_to_delete.begin(), m_peers_to_delete.end(), peer) == m_peers_to_delete.end()) {
					//marked for delection, dec reference and delete when zero
					it = m_connections.erase(it);
					peer->DecRef();
					m_server->UnregisterSocket(peer);
					m_peers_to_delete.push_back(peer);
					continue;
				}
				it++;
			}

			it = m_peers_to_delete.begin();
			while (it != m_peers_to_delete.end()) {
				GP::Peer *p = *it;
				if (p->GetRefCount() == 0) {
					delete p;
					it = m_peers_to_delete.erase(it);
					continue;
				}
				it++;
			}
		}

		TickConnections();
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


	int Driver::GetNumConnections() {
		return m_connections.size();
	}

	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}
	Peer *Driver::FindPeerByProfileID(int profileid) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			if(p->GetProfileID() == profileid) {
				return p;
			}
			it++;
		}
		return NULL;
	}
	void Driver::InformStatusUpdate(int from_profileid, GPShared::GPStatus status) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->inform_status_update(from_profileid, status);
			it++;
		}
	}
	const std::vector<INetPeer *> Driver::getPeers() {
		std::vector<INetPeer *> peers;
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			peers.push_back((INetPeer *)*it);
			it++;
		}
		return peers;
	}
	const std::vector<int> Driver::getSockets() {
		std::vector<int> sockets;
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			sockets.push_back(p->GetSocket());
			it++;
		}
		return sockets;
	}
}
