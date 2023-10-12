#include "SBDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>

#include "SBServer.h"

#include "SBPeer.h"
#include "V1Peer.h"
#include "V2Peer.h"

#include <tasks/tasks.h>
namespace SB {
	
	Driver::Driver(INetServer *server, const char *host, uint16_t port, int version, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {
		m_version = version;
		uv_mutex_init(&m_mutex);
	}
	Driver::~Driver() {
		uv_mutex_destroy(&m_mutex);
	}
	void Driver::TickConnections() { 
		TCPDriver::TickConnections();
		MM::Server serv;
		uv_mutex_lock(&m_mutex);
		while (!m_server_delete_queue.empty()) {
			serv = m_server_delete_queue.front();
			m_server_delete_queue.pop();
			SendDeleteServer(&serv);
		}

		while (!m_server_new_queue.empty()) {
			serv = m_server_new_queue.front();
			m_server_new_queue.pop();
			SendNewServer(&serv);
		}
		while (!m_server_update_queue.empty()) {
			serv = m_server_update_queue.front();
			m_server_update_queue.pop();
			SendUpdateServer(&serv);
		}
		uv_mutex_unlock(&m_mutex);
	}
	bool Driver::LLIterator_SendDeleteServer(INetPeer* peer, MM::Server* server) {
		((Peer*)peer)->informDeleteServers(server);
		return true;
	}
	bool Driver::LLIterator_SendNewServer(INetPeer* peer, MM::Server* server) {
		((Peer*)peer)->informNewServers(server);
		return true;
	}
	bool Driver::LLIterator_SendUpdateServer(INetPeer* peer, MM::Server* server) {
		((Peer*)peer)->informUpdateServers(server);
		return true;
	}
	void Driver::SendDeleteServer(MM::Server *server) {
		OS::LinkedListIterator<INetPeer*, MM::Server*> iterator(GetPeerList());
		iterator.Iterate(LLIterator_SendDeleteServer, server);
	}
	void Driver::SendNewServer(MM::Server *server) {
		OS::LinkedListIterator<INetPeer*, MM::Server*> iterator(GetPeerList());
		iterator.Iterate(LLIterator_SendNewServer, server);
	}
	void Driver::SendUpdateServer(MM::Server *server) {
		OS::LinkedListIterator<INetPeer*, MM::Server*> iterator(GetPeerList());
		iterator.Iterate(LLIterator_SendUpdateServer, server);
	}
	void Driver::AddDeleteServer(MM::Server serv) {
		uv_mutex_lock(&m_mutex);
		m_server_delete_queue.push(serv);
		uv_mutex_unlock(&m_mutex);
	}
	void Driver::AddNewServer(MM::Server serv) {
		uv_mutex_lock(&m_mutex);
		m_server_new_queue.push(serv);
		uv_mutex_unlock(&m_mutex);
	}
	void Driver::AddUpdateServer(MM::Server serv) {
		uv_mutex_lock(&m_mutex);
		m_server_update_queue.push(serv);
		uv_mutex_unlock(&m_mutex);
	}
	INetPeer *Driver::CreatePeer(uv_tcp_t *listener_socket) {
		Peer *peer = NULL;

		switch (m_version) {
		case 1:
			peer = new V1Peer(this, listener_socket);
			break;
		case 2:
			peer = new V2Peer(this, listener_socket);
			break;
		}
		return peer;
	}
}
