#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>

#include "QRServer.h"
#include <OS/legacy/buffwriter.h>

#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/socketlib/socketlib.h>
#include "V1Peer.h"
#include "V2Peer.h"
namespace QR {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		
		Socket::Init();
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
			//signal error
		}

		m_local_addr.sin_port = Socket::htons(port);
		m_local_addr.sin_addr.s_addr = Socket::htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = Socket::bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
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
	void Driver::think(fd_set *fdset) {
		TickConnections(fdset);

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			if (!peer->ShouldDelete()) {
				peer->think();
			}
			else if (std::find(m_peers_to_delete.begin(), m_peers_to_delete.end(), peer) == m_peers_to_delete.end()) {
				//marked for delection, dec reference and delete when zero
				it = m_connections.erase(it);
				peer->DecRef();
				m_peers_to_delete.push_back(peer);
				continue;
			}
			it++;
		}

		it = m_peers_to_delete.begin();
		while (it != m_peers_to_delete.end()) {
			QR::Peer *p = *it;
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
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		return NULL;
	}
	Peer *Driver::find_or_create(struct sockaddr_in *address, int version) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			const struct sockaddr_in *peer_address = peer->getAddress();
			if (address->sin_port == peer_address->sin_port && address->sin_addr.s_addr == peer_address->sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		Peer *ret = NULL;
		switch(version) {
			case 1:
				ret = new V1Peer(this, address, m_sd);
				break;
			case 2:
				ret = new V2Peer(this, address, m_sd);
				break;
		}
		m_connections.push_back(ret);
		return ret;
	}
	void Driver::tick(fd_set *fdset) {

		TickConnections(fdset);
		if (!FD_ISSET(m_sd, fdset)) {
			return;
		}
		char recvbuf[MAX_DATA_SIZE + 1];

		struct sockaddr_in si_other;
		socklen_t slen = sizeof(struct sockaddr_in);

		int len = recvfrom(m_sd,(char *)&recvbuf,sizeof(recvbuf),0,(struct sockaddr *)&si_other,&slen);
		
		Peer *peer = NULL;
		if(len > 0) {
			peer = find_or_create(&si_other, recvbuf[0] == '\\' ? 1 : 2);
		} else {
			peer = find_client(&si_other);
		}
		if(peer) {
			if(len > 0) {
				recvbuf[len] = 0;
				peer->handle_packet((char *)&recvbuf, len);
			} else if(len < 0) {
				peer->SetDelete(true);
			}
		}


	}


	int Driver::getListenerSocket() {
		return m_sd;
	}

	uint32_t Driver::getBindIP() {
		return htonl(m_local_addr.sin_addr.s_addr);
	}

	int Driver::GetNumConnections() {
		return m_connections.size();
	}


	int Driver::setup_fdset(fd_set *fdset) {

		int hsock = m_sd;
		FD_SET(m_sd, fdset);
		
		/* not needed because UDP
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			int sd = p->GetSocket();
			FD_SET(sd, fdset);
			if (sd > hsock)
				hsock = sd;
			it++;
		}
		*/
		return hsock;
	}

	void Driver::TickConnections(fd_set *fdset) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think();
			it++;
		}
	}
}