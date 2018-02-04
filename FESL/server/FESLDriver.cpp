#include "FESLDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "FESLServer.h"
#include "FESLPeer.h"


namespace FESL {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, bool use_ssl, const char *x509_path, const char *rsa_priv_path, EFESLSSL_Type ssl_version) : INetDriver(server) {
		uint32_t bind_ip = INADDR_ANY;
		
		if ((m_sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
			//signal error
		}
		int on = 1;
		if (setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))
			< 0) {
			//signal error
		}

		m_local_addr.sin_port = htons(port);
		m_local_addr.sin_addr.s_addr = htonl(bind_ip);
		m_local_addr.sin_family = AF_INET;
		int n = bind(m_sd, (struct sockaddr *)&m_local_addr, sizeof m_local_addr);
		if (n < 0) {
			//signal error
		}
		if (listen(m_sd, SOMAXCONN)
			< 0) {
			//signal error
		}

		mp_rsa_key_data = NULL;
		mp_x509_cert_data = NULL;

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(Driver::TaskThread, this, true);

		makeNonBlocking(m_sd);

		if (use_ssl) {
			switch (ssl_version) {
				case EFESLSSL_SSLv23:
					m_ssl_ctx = SSL_CTX_new(SSLv23_method());
					break;
				# ifndef OPENSSL_NO_SSL2_METHOD
				case EFESLSSL_SSLv2:
					m_ssl_ctx = SSL_CTX_new(SSLv2_method());
					break;
				#endif
				# ifndef OPENSSL_NO_SSL3_METHOD
				case EFESLSSL_SSLv3:
					m_ssl_ctx = SSL_CTX_new(SSLv3_method());
					break;
				#endif
				case EFESLSSL_TLS10:
					m_ssl_ctx = SSL_CTX_new(TLSv1_method());
					break;
				case EFESLSSL_TLS11:
					m_ssl_ctx = SSL_CTX_new(TLSv1_1_method());
					break;
				case EFESLSSL_TLS12:
					m_ssl_ctx = SSL_CTX_new(TLSv1_2_method());
					break;
				default:
					fprintf(stderr, "\nUnknown SSL type\n");
					exit(1);
					break;
			}

			SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL");
			SSL_CTX_set_options(m_ssl_ctx, SSL_OP_ALL);

			FILE *fd = fopen(x509_path, "rb");
			fseek(fd, 0, SEEK_END);
			int x509_len = ftell(fd);
			fseek(fd, 0, SEEK_SET);

			mp_x509_cert_data = malloc(x509_len);
			fread(mp_x509_cert_data, x509_len, 1, fd);
			fclose(fd);

			fd = fopen(rsa_priv_path, "rb");
			fseek(fd, 0, SEEK_END);
			int rsa_len = ftell(fd);
			fseek(fd, 0, SEEK_SET);

			mp_rsa_key_data = malloc(rsa_len);
			fread(mp_rsa_key_data, rsa_len, 1, fd);
			fclose(fd);

			if (!SSL_CTX_use_certificate_ASN1(m_ssl_ctx, x509_len, (const unsigned char *)mp_x509_cert_data) ||
				!SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_RSA, m_ssl_ctx, (const unsigned char *)mp_rsa_key_data, rsa_len)) {
				fprintf(stderr, "\nError: problems with the loading of the certificate in memory\n");
				exit(1);
			}
			SSL_CTX_set_verify_depth(m_ssl_ctx, 1);
		}
		else {
			m_ssl_ctx = NULL;
		}
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

		if(m_ssl_ctx)
			SSL_CTX_free(m_ssl_ctx);

		if (mp_x509_cert_data) {
			free(mp_x509_cert_data);
		}
		if (mp_rsa_key_data) {
			free(mp_rsa_key_data);
		}
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

					driver->m_stats_queue.push(peer->GetPeerStats());
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
			while (true) {
				socklen_t psz = sizeof(struct sockaddr_in);
				struct sockaddr_in peer;
				int sda = accept(m_sd, (struct sockaddr *)&peer, &psz);
				if (sda <= 0) return;
				Peer *mp_peer = new Peer(this, &peer, sda);

				makeNonBlocking(mp_peer);

				mp_mutex->lock();
				m_connections.push_back(mp_peer);
				m_server->RegisterSocket(mp_peer);
				mp_mutex->unlock();
				//mp_peer->think(true);
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

	int Driver::GetNumConnections() {
		return m_connections.size();
	}

	const std::vector<int> Driver::getSockets() {
		std::vector<int> sockets;
		mp_mutex->lock();
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			sockets.push_back(p->GetSocket());
			it++;
		}
		mp_mutex->unlock();
		return sockets;
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
	
	void Driver::TickConnections() {
		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = *it;
			p->think(false);
			it++;
		}
	}
	OS::MetricInstance Driver::GetMetrics() {
		OS::MetricInstance peer_metric;
		OS::MetricValue arr_value2, value, peers;

		mp_mutex->lock();

		std::vector<Peer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			INetPeer * peer = (INetPeer *)*it;
			OS::Address address = *peer->getAddress();
			value = peer->GetMetrics().value;

			value.key = address.ToString(false);

			peers.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, value));
			it++;
		}

		while (!m_stats_queue.empty()) {
			PeerStats stats = m_stats_queue.front();
			m_stats_queue.pop();
			peers.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, Peer::GetMetricItemFromStats(stats)));
		}

		peers.key = "peers";
		arr_value2.type = OS::MetricType_Array;
		peers.type = OS::MetricType_Array;
		arr_value2.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_Array, peers));


		peer_metric.key = OS::Address(m_local_addr).ToString(false);
		arr_value2.key = peer_metric.key;
		peer_metric.value = arr_value2;

		mp_mutex->unlock();
		return peer_metric;
	}
}