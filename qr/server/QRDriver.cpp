#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"

#include "QRPeer.h"
#include "QRDriver.h"
#include "V1Peer.h"
#include "V2Peer.h"
namespace QR {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		OS::Address bind_address(0, port);
		mp_socket = getNetIOInterface()->BindUDP(bind_address);

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(Driver::TaskThread, this, true);

	}
	Driver::~Driver() {
		mp_thread->SignalExit(true);
		delete mp_thread;
		delete mp_mutex;

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			m_server->UnregisterSocket(peer);
			delete peer;
			it++;
		}
		getNetIOInterface()->closeSocket(mp_socket);
	}
	void *Driver::TaskThread(OS::CThread *thread) {
		Driver *driver = (Driver *)thread->getParams();
		while(thread->isRunning()) {
			driver->mp_mutex->lock();
			driver->TickConnections();
			std::vector<Peer *>::iterator it = driver->m_connections.begin();
			while (it != driver->m_connections.end()) {
				Peer *peer = *it;
				if (peer->ShouldDelete() && std::find(driver->m_peers_to_delete.begin(), driver->m_peers_to_delete.end(), peer) == driver->m_peers_to_delete.end()) {
					//marked for delection, dec reference and delete when zero
					it = driver->m_connections.erase(it);
					driver->m_peers_to_delete.push_back(peer);

					driver->m_server->UnregisterSocket(peer);
					peer->DeleteServer();
					peer->DecRef();
					continue;
				}
				it++;
			}

			it = driver->m_peers_to_delete.begin();
			while (it != driver->m_peers_to_delete.end()) {
				QR::Peer *p = *it;
				if (p->GetRefCount() == 0) {
					delete p;
					it = driver->m_peers_to_delete.erase(it);
					continue;
				}
				it++;
			}
			driver->mp_mutex->unlock();
			OS::Sleep(DRIVER_THREAD_TIME);
		}
		return NULL;
	}
	void Driver::think(bool listener_waiting) {
		mp_mutex->lock();
		TickConnections();
		if (listener_waiting) {

			std::vector<INetIODatagram> datagrams;
			getNetIOInterface()->datagramRecv(mp_socket, datagrams);
			std::vector<INetIODatagram>::iterator it = datagrams.begin();
			while (it != datagrams.end()) {
				INetIODatagram dgram = *it;
				Peer *peer = NULL;
				if (dgram.comm_len == 0) {
					it++;
					continue;
				}
				if (dgram.error_flag) {
					peer = find_client(dgram.address);
					if (peer) {
						peer->Delete();
					}
				}
				else {
					peer = find_or_create(dgram.address, mp_socket, ((const char *)dgram.buffer.GetHead())[0] == '\\' ? 1 : 2);
					peer->handle_packet(dgram);
				}

				it++;
			}
		}
		mp_mutex->unlock();
	}


	Peer *Driver::find_client(OS::Address address) {
		mp_mutex->lock();
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			OS::Address peer_address = peer->getAddress();
			if (address == peer_address && !peer->ShouldDelete()) {
				mp_mutex->unlock();
				return peer;
			}
			it++;
		}
		mp_mutex->unlock();
		return NULL;
	}
	Peer *Driver::find_or_create(OS::Address address, INetIOSocket *socket, int version) {
		Peer *ret = find_client(address);
		if(ret) return ret;
		INetIOSocket *client_socket = new INetIOSocket();
		client_socket->address = address;
		client_socket->sd = socket->sd;
		client_socket->shared_socket = true;

		switch (version) {
		case 1:
			ret = new V1Peer(this, client_socket);
			break;
		case 2:
			ret = new V2Peer(this, client_socket);
			break;
		}
		m_server->RegisterSocket(ret);
		mp_mutex->lock();
		m_connections.push_back(ret);
		ret->OnConnectionReady();
		mp_mutex->unlock();
		return ret;
	}

	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}
	INetIOSocket *Driver::getListenerSocket() const {
		return mp_socket;
	}
	const std::vector<INetIOSocket *> Driver::getSockets() const {
		std::vector<INetIOSocket *> ret;
		ret.push_back(mp_socket);
		return ret;
	}
	const std::vector<INetPeer *> Driver::getPeers(bool inc_ref) {
		std::vector<INetPeer *> peers;
		mp_mutex->lock();
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer *p = (INetPeer *)*it;
			if (inc_ref) {
				p->IncRef();
			}
			peers.push_back(p);
			it++;
		}
		mp_mutex->unlock();
		return peers;
	}
	void Driver::OnPeerMessage(INetPeer *peer) {

	}
}