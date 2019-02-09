#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"


namespace FESL {
	Driver::Driver(INetServer *server, OS::Address address, PublicInfo public_info, std::string str_crypter_rsa_key, const char *x509_path, const char *rsa_priv_path, SSLNetIOIFace::ESSL_Type ssl_version) : INetDriver(server) {

		//setup config vars
		m_server_info.domainPartition = public_info.domainPartition;
		m_server_info.subDomain = public_info.subDomain;
		m_server_info.messagingHostname = public_info.messagingHostname;
		m_server_info.messagingPort = public_info.messagingPort;
		m_server_info.theaterHostname = public_info.theaterHostname;
		m_server_info.theaterPort = public_info.theaterPort;
		m_server_info.termsOfServiceData = public_info.termsOfServiceData;

		mp_socket_interface = new SSLNetIOIFace::SSLNetIOInterface(ssl_version, rsa_priv_path, x509_path);		

		mp_socket = mp_socket_interface->BindTCP(address);
		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(Driver::TaskThread, this, true);

		mp_string_crypter = new OS::StringCrypter(str_crypter_rsa_key);
	}
	Driver::~Driver() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			delete peer;
			it++;
		}

		delete mp_mutex;
		delete mp_thread;
		delete mp_string_crypter;
	}
	void *Driver::TaskThread(OS::CThread *thread) {
		Driver *driver = (Driver *)thread->getParams();
		for (;;) {
			driver->mp_mutex->lock();
			std::vector<Peer *>::iterator it = driver->m_connections.begin();
			while (it != driver->m_connections.end()) {
				Peer *peer = *it;
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
				FESL::Peer *p = *it;
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
	}
	void Driver::think(bool listener_waiting) {
		if (listener_waiting) {
			std::vector<INetIOSocket *> sockets = getSSL_Socket_Interface()->TCPAccept(mp_socket);
			std::vector<INetIOSocket *>::iterator it = sockets.begin();
			while (it != sockets.end()) {
				INetIOSocket *sda = *it;
				if (sda == NULL) return;
				Peer *mp_peer = NULL;

				mp_peer = new Peer(this, sda);

				mp_mutex->lock();
				m_connections.push_back(mp_peer);
				m_server->RegisterSocket(mp_peer);
				mp_mutex->unlock();
				it++;
			}
		}
	}

	const std::vector<INetPeer *> Driver::getPeers(bool inc_ref) {
		mp_mutex->lock();
		std::vector<INetPeer *> peers;
		std::vector<Peer *>::iterator it = m_connections.begin();
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

	const std::vector<INetIOSocket *> Driver::getSockets() const {
		std::vector<INetIOSocket *> ret;
		std::vector<Peer *>::const_iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			ret.push_back(peer->GetSocket());
			it++;
		}
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

	SSLNetIOIFace::SSLNetIOInterface *Driver::getSSL_Socket_Interface() {
		return mp_socket_interface;
	}
	void Driver::OnUserAuth(OS::Address remote_address, int userid, int profileid) {
		mp_mutex->lock();
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = *it;
			OS::User user;
			OS::Profile profile;
			if(peer->GetAuthCredentials(user, profile)) {
				if(user.id == userid && peer->getAddress() != remote_address) {
					peer->DuplicateLoginExit();
				}
			}
			it++;
		}
		mp_mutex->unlock();
	}
}