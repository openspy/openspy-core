#include <stdio.h>
#include <stdlib.h>

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

		gettimeofday(&m_server_start, NULL);

		NN::NNQueryTask::getQueryTask()->AddDriver(this);


	}
	Driver::~Driver() {
		NN::NNQueryTask::getQueryTask()->RemoveDriver(this);
	}
	void Driver::think(fd_set *fdset) {
		TickConnections(fdset);

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			if (!peer->ShouldDelete())
				peer->think();
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
			const struct sockaddr_in peer_address = peer->getAddress().GetInAddr();
			if (address->sin_port == peer_address.sin_port && address->sin_addr.s_addr == peer_address.sin_addr.s_addr) {
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
			const struct sockaddr_in peer_address = peer->getAddress().GetInAddr();
			if (address->sin_port == peer_address.sin_port && address->sin_addr.s_addr == peer_address.sin_addr.s_addr) {
				return peer;
			}
			it++;
		}
		Peer *ret = NULL;
		/*
		printf("Creating peer version %d\n", version);
		switch(version) {
			case 1:
				ret = new V1Peer(this, address, m_sd);
				break;
			case 2:
				ret = new V2Peer(this, address, m_sd);
				break;
		}
		*/
		ret = new Peer(this, address, m_sd);
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
		recvbuf[len] = 0;
		Peer *peer = find_or_create(&si_other, recvbuf[0] == '\\' ? 1 : 2);
		peer->handle_packet((char *)&recvbuf, len);


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
		return hsock + 1;
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