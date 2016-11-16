#include "SBDriver.h"
#include <stdio.h>
#include <stdlib.h>

#include "SBServer.h"
#include <OS/legacy/buffwriter.h>

#include "SBPeer.h"
#include <OS/socketlib/socketlib.h>

namespace SB {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		
		Socket::Init();
		MM::Init(this);
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = Socket::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (Socket::setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;

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

	}
	void Driver::think(fd_set *fdset) {

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			if (!peer->ShouldDelete())
				peer->think(FD_ISSET(peer->GetSocket(), fdset));
			else {
				//delete if marked for deletiontel
				delete peer;
				it = m_connections.erase(it);
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
	void Driver::tick(fd_set *fdset) {

		TickConnections(fdset);
		if (!FD_ISSET(m_sd, fdset)) {
			return;
		}
		socklen_t psz = sizeof(struct sockaddr_in);
		struct sockaddr_in peer;
		int sda = Socket::accept(m_sd, (struct sockaddr *)&peer, &psz);
		if (sda <= 0) return;
		Peer *mp_peer = new Peer(this, &peer, sda);
		m_connections.push_back(mp_peer);
		mp_peer->think(true);

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


	int Driver::setup_fdset(fd_set *fdset) {

		int hsock = m_sd;
		FD_SET(m_sd, fdset);
		
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			int sd = p->GetSocket();
			FD_SET(sd, fdset);
			if (sd > hsock)
				hsock = sd;
			it++;
		}
		return hsock + 1;
	}

	void Driver::TickConnections(fd_set *fdset) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(FD_ISSET(p->GetSocket(), fdset));
			it++;
		}
	}
	void Driver::SendDeleteServer(MM::ServerListQuery servers) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			printf("Inform client delete\n");
			p->informDeleteServers(servers);
			it++;
		}
	}
	void Driver::SendNewServer(MM::ServerListQuery servers) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informNewServers(servers);
			it++;
		}
	}
	void Driver::SendUpdateServer(MM::ServerListQuery servers) {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informUpdateServers(servers);
			it++;
		}
	}
}