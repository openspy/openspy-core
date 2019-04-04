#include <algorithm>
#include "TCPDriver.h"

    TCPDriver::TCPDriver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		OS::Address bind_address(0, port);
		mp_socket = server->getNetIOInterface()->BindTCP(bind_address);

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(TCPDriver::TaskThread, this, true);
    }
    TCPDriver::~TCPDriver() {
		mp_thread->SignalExit(true);
		delete mp_mutex;
		delete mp_thread;

		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer *peer = *it;
			delete peer;
			it++;
		}
    }
    void TCPDriver::think(bool listener_waiting) {
		if (listener_waiting) {
			std::vector<INetIOSocket *> sockets = getServer()->getNetIOInterface()->TCPAccept(mp_socket);
			std::vector<INetIOSocket *>::iterator it = sockets.begin();
			while (it != sockets.end()) {
				INetIOSocket *sda = *it;
				if (sda == NULL) return;
				INetPeer *mp_peer = CreatePeer(sda);//new INetPeer(this, sda);

				mp_mutex->lock();
				m_connections.push_back(mp_peer);
				m_server->RegisterSocket(mp_peer);
				mp_mutex->unlock();
				it++;
			}
		}
	}

    const std::vector<INetPeer *> TCPDriver::getPeers(bool inc_ref = false) {
		mp_mutex->lock();
		std::vector<INetPeer *> peers;
		std::vector<INetPeer *>::iterator it = m_connections.begin();
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
    INetIOSocket *TCPDriver::getListenerSocket() const {
        return mp_socket;
    }
    const std::vector<INetIOSocket *> TCPDriver::getSockets() const {
		std::vector<INetIOSocket *> sockets;
		mp_mutex->lock();
		std::vector<INetPeer *>::const_iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer *p = *it;
			sockets.push_back(p->GetSocket());
			it++;
		}
		mp_mutex->unlock();
		return sockets;
    }
    
    void *TCPDriver::TaskThread(OS::CThread *thread) {
		TCPDriver *driver = (TCPDriver *)thread->getParams();
		while (thread->isRunning()) {
			driver->mp_mutex->lock();
			std::vector<INetPeer *>::iterator it = driver->m_connections.begin();
			while (it != driver->m_connections.end()) {
				INetPeer *peer = *it;
				if (peer->ShouldDelete() && std::find(driver->m_peers_to_delete.begin(), driver->m_peers_to_delete.end(), peer) == driver->m_peers_to_delete.end()) {
					//marked for delection, dec reference and delete when zero
					it = driver->m_connections.erase(it);
					peer->DecRef();

					driver->m_server->UnregisterSocket(peer);

					driver->m_peers_to_delete.push_back(peer);
					continue;
				}
				it++;
			}

			it = driver->m_peers_to_delete.begin();
			while (it != driver->m_peers_to_delete.end()) {
				INetPeer *p = *it;
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
		return NULL;
    }
    void TCPDriver::TickConnections() {
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer *p = *it;
			p->think(false);
			it++;
		}
    }
