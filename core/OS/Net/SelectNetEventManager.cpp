#include <OS/OpenSpy.h>

#if EVTMGR_USE_SELECT

#include "SelectNetEventManager.h"
#include <stdio.h>
#include "NetPeer.h"
SelectNetEventManager::SelectNetEventManager() {
	m_exit_flag = false;
	m_dirty_fdset = true;
	mp_mutex = OS::CreateMutex();
}
SelectNetEventManager::~SelectNetEventManager() {
	m_exit_flag = true;
	delete mp_mutex;
}
void SelectNetEventManager::run() {

	struct timeval timeout;

	memset(&timeout, 0, sizeof(struct timeval));
	timeout.tv_sec = SELECT_TIMEOUT;

	int hsock = setup_fdset();

	if (select(hsock, &m_fdset, NULL, NULL, &timeout) < 0) {
		//return;
	}
	if (m_exit_flag) {
		return;
	}
	mp_mutex->lock();

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while (it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if (FD_ISSET(driver->getListenerSocket()->sd, &m_fdset))
			driver->think(true);
		it++;
	}

	std::vector<INetPeer *>::iterator it2 = m_peers.begin();
	while (it2 != m_peers.end()) {
		INetPeer *peer = *it2;
		INetIOSocket *sd = peer->GetSocket();
		if (sd != peer->GetDriver()->getListenerSocket() && FD_ISSET(sd->sd, &m_fdset))
			peer->think(true);
		it2++;
	}
	mp_mutex->unlock();
}
int SelectNetEventManager::setup_fdset() {
	int hsock = -1;
	mp_mutex->lock();
	FD_ZERO(&m_fdset);
	if (m_dirty_fdset) {
		m_cached_sockets.clear();
		std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
		while (it != m_net_drivers.end()) {
			INetDriver *driver = *it;
			INetIOSocket *sd = driver->getListenerSocket();
			m_cached_sockets.push_back(sd->sd);
			FD_SET(sd->sd, &m_fdset);
			it++;
		}

		std::vector<INetPeer*>::iterator it2 = m_peers.begin();
		while (it2 != m_peers.end()) {
			INetPeer *peer = *it2;
			INetIOSocket *sd = peer->GetSocket();
			m_cached_sockets.push_back(sd->sd);
			FD_SET(sd->sd, &m_fdset);
			it2++;
		}
		m_hsock = hsock + 1;
		m_dirty_fdset = false;
		mp_mutex->unlock();
		return m_hsock;
	}
	else {
		std::vector<int>::iterator it = m_cached_sockets.begin();
		while (it != m_cached_sockets.end()) {
			int sd = *it;
			FD_SET(sd, &m_fdset);
			it++;
		}
		mp_mutex->unlock();
		return m_hsock;
	}
}
void SelectNetEventManager::RegisterSocket(INetPeer *peer) {
	mp_mutex->lock();
	m_dirty_fdset = true;
	m_peers.push_back(peer);
	mp_mutex->unlock();
}
void SelectNetEventManager::UnregisterSocket(INetPeer *peer) {
	mp_mutex->lock();
	std::vector<INetPeer *>::iterator it = std::find(m_peers.begin(), m_peers.end(), peer);
	if (it != m_peers.end()) {
		m_peers.erase(it);
	}

	m_dirty_fdset = true;
	mp_mutex->unlock();
}

//NET IO INTERFACE
INetIOSocket *SelectNetEventManager::BindTCP(OS::Address bind_address) {
	INetIOSocket *net_socket = (INetIOSocket *)malloc(sizeof(INetIOSocket));
	net_socket->address = bind_address;

	uint32_t bind_ip = INADDR_ANY;

	if ((net_socket->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		//signal error
		goto end_error;
	}
	int on = 1;
	if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
		< 0) {
		//signal error
		goto end_error;
	}
#if SO_REUSEPORT
	if (setsockopt(net_socket->sd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on))
		< 0) {
		//signal error
		goto end_error;
	}
#endif
	struct sockaddr_in addr;

	addr = bind_address.GetInAddr();
	addr.sin_family = AF_INET;
	int n = bind(net_socket->sd, (struct sockaddr *)&addr, sizeof addr);
	if (n < 0) {
		//signal error
		goto end_error;
	}
	if (listen(net_socket->sd, SOMAXCONN)
		< 0) {
		//signal error
		goto end_error;
	}
	makeNonBlocking(net_socket->sd);
	return net_socket;
end_error:
	if(net_socket)
		free((void *)net_socket);
}
std::vector<INetIOSocket *> SelectNetEventManager::TCPAccept(INetIOSocket *socket) {
	std::vector<INetIOSocket *> ret;
	INetIOSocket *incoming_socket;
	while (true) {
		socklen_t psz = sizeof(struct sockaddr_in);
		struct sockaddr_in peer;
		int sda = accept(socket->sd, (struct sockaddr *)&peer, &psz);
		if (sda <= 0) break;
		makeNonBlocking(sda);
		incoming_socket = (INetIOSocket *)malloc(sizeof(INetIOSocket));
		incoming_socket->sd = sda;
		incoming_socket->address = peer;
		ret.push_back(incoming_socket);
	}
	return ret;
}
int SelectNetEventManager::streamRecv(INetIOSocket *socket, OS::Buffer &buffer) {
	char recvbuf[1492];
	int len = recv(socket->sd, recvbuf, sizeof recvbuf, MSG_NOSIGNAL);
	if (len <= 0) goto end;
	buffer.WriteBuffer(recvbuf, len);
	end:
	return len;
}
int SelectNetEventManager::streamSend(INetIOSocket *socket, OS::Buffer &buffer) {
	return send(socket->sd, (const char *)buffer.GetHead(), buffer.size(), MSG_NOSIGNAL);
}

INetIOSocket *SelectNetEventManager::BindUDP(OS::Address bind_address) {
	return NULL;
}
int SelectNetEventManager::datagramRecv(INetIOSocket *socket, OS::Buffer &buffer, OS::Address &data_source) {
	return 0;
}
int SelectNetEventManager::datagramSend(INetIOSocket *socket, OS::Buffer &buffer, OS::Address &data_dest) {
	return 0;
}
void SelectNetEventManager::closeSocket(INetIOSocket *socket) {
	close(socket->sd);
	free((void *)socket);
}
void SelectNetEventManager::makeNonBlocking(int sd) {
	unsigned long mode = 1;
#ifdef _WIN32
	ioctlsocket(sd, FIONBIO, &mode);
#elif defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (mode = fcntl(sd, F_GETFL, 0)))
		mode = 0;
	mode |= O_NONBLOCK;
	fcntl(sd, F_SETFL, mode);
#else
	/* Otherwise, use the old way of doing it */
	ioctl(sd, FIONBIO, &mode);
#endif
}
#endif