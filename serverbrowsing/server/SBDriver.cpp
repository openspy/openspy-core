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
	}
	void Driver::TickConnections() { 
		TCPDriver::TickConnections();
		MM::Server serv;
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
	}
	void Driver::SendDeleteServer(MM::Server *server) {
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = (Peer *)*it;
			p->informDeleteServers(server);
			it++;
		}
	}
	void Driver::SendNewServer(MM::Server *server) {
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = (Peer *)*it;
			p->informNewServers(server);
			it++;
		}
	}
	void Driver::SendUpdateServer(MM::Server *server) {
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->informUpdateServers(server);
			it++;
		}
	}
	void Driver::AddDeleteServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_delete_queue.push(serv);
		mp_mutex->unlock();
	}
	void Driver::AddNewServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_new_queue.push(serv);
		mp_mutex->unlock();
	}
	void Driver::AddUpdateServer(MM::Server serv) {
		mp_mutex->lock();
		m_server_update_queue.push(serv);
		mp_mutex->unlock();
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		Peer *peer = NULL;

		switch (m_version) {
		case 1:
			peer = new V1Peer(this, socket);
			break;
		case 2:
			peer = new V2Peer(this, socket);
			break;
		}
		return peer;
	}
}
