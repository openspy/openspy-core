#include "FESLDriver.h"
#include <stdio.h>
#include <stdlib.h>

#include "FESLServer.h"
#include "FESLPeer.h"

static const uint8_t SSL_CERT_X509[] =   // x509 �in input.crt �inform PEM �out output.crt �outform DER
"\x30\x82\x03\x07\x30\x82\x02\x70\xa0\x03\x02\x01\x02\x02\x09\x00"
"\x85\x3a\x6e\x0a\xa4\x3c\x6b\xec\x30\x0d\x06\x09\x2a\x86\x48\x86"
"\xf7\x0d\x01\x01\x05\x05\x00\x30\x61\x31\x0b\x30\x09\x06\x03\x55"
"\x04\x06\x13\x02\x55\x53\x31\x0b\x30\x09\x06\x03\x55\x04\x08\x14"
"\x02\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04\x07\x14\x02\x22\x22"
"\x31\x0b\x30\x09\x06\x03\x55\x04\x0a\x14\x02\x22\x22\x31\x0b\x30"
"\x09\x06\x03\x55\x04\x0b\x14\x02\x22\x22\x31\x0b\x30\x09\x06\x03"
"\x55\x04\x03\x14\x02\x22\x22\x31\x11\x30\x0f\x06\x09\x2a\x86\x48"
"\x86\xf7\x0d\x01\x09\x01\x16\x02\x22\x22\x30\x1e\x17\x0d\x30\x39"
"\x30\x31\x30\x34\x30\x33\x31\x34\x33\x33\x5a\x17\x0d\x31\x30\x30"
"\x31\x30\x34\x30\x33\x31\x34\x33\x33\x5a\x30\x61\x31\x0b\x30\x09"
"\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x0b\x30\x09\x06\x03\x55"
"\x04\x08\x14\x02\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04\x07\x14"
"\x02\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04\x0a\x14\x02\x22\x22"
"\x31\x0b\x30\x09\x06\x03\x55\x04\x0b\x14\x02\x22\x22\x31\x0b\x30"
"\x09\x06\x03\x55\x04\x03\x14\x02\x22\x22\x31\x11\x30\x0f\x06\x09"
"\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x02\x22\x22\x30\x81\x9f"
"\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00\x03"
"\x81\x8d\x00\x30\x81\x89\x02\x81\x81\x00\xc5\xe3\x3f\x2d\x8f\x98"
"\xc2\x2a\xef\x71\xea\x40\x21\x54\x3f\x08\x62\x9c\x7b\x39\x22\xfd"
"\xda\x80\x1f\x21\x3e\x8d\x68\xcf\x8e\x6b\x70\x98\x95\x2c\x1e\x4e"
"\x79\x39\x45\xf5\xa3\xd9\x20\x54\x85\x79\x36\xf5\x08\xbe\xa0\xa6"
"\x03\x80\x60\x21\xd6\xbc\xde\xf8\xed\xe8\x73\x02\x96\x84\xcb\xb4"
"\xff\x72\x89\xf4\x56\x41\xf6\x28\xf6\x6b\x9f\x0c\x1d\xe0\x9b\x21"
"\xcb\x86\x08\xdf\x6b\xc1\x8a\xd6\xa3\x52\x2f\xfa\xd8\x5a\x2c\x86"
"\x52\x0d\x75\x2d\xf6\x17\x11\xa7\x17\xad\xc2\x3b\xd8\x0f\xcf\xb7"
"\x2b\x2c\x8a\xc4\xcd\x2d\x94\xe4\x15\x75\x02\x03\x01\x00\x01\xa3"
"\x81\xc6\x30\x81\xc3\x30\x1d\x06\x03\x55\x1d\x0e\x04\x16\x04\x14"
"\x00\x6b\x12\xa2\xb9\x10\x90\xe4\xe5\xe8\xff\xec\x5c\x24\x44\xee"
"\xed\xc1\x66\xb7\x30\x81\x93\x06\x03\x55\x1d\x23\x04\x81\x8b\x30"
"\x81\x88\x80\x14\x00\x6b\x12\xa2\xb9\x10\x90\xe4\xe5\xe8\xff\xec"
"\x5c\x24\x44\xee\xed\xc1\x66\xb7\xa1\x65\xa4\x63\x30\x61\x31\x0b"
"\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55\x53\x31\x0b\x30\x09\x06"
"\x03\x55\x04\x08\x14\x02\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04"
"\x07\x14\x02\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04\x0a\x14\x02"
"\x22\x22\x31\x0b\x30\x09\x06\x03\x55\x04\x0b\x14\x02\x22\x22\x31"
"\x0b\x30\x09\x06\x03\x55\x04\x03\x14\x02\x22\x22\x31\x11\x30\x0f"
"\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x09\x01\x16\x02\x22\x22\x82"
"\x09\x00\x85\x3a\x6e\x0a\xa4\x3c\x6b\xec\x30\x0c\x06\x03\x55\x1d"
"\x13\x04\x05\x30\x03\x01\x01\xff\x30\x0d\x06\x09\x2a\x86\x48\x86"
"\xf7\x0d\x01\x01\x05\x05\x00\x03\x81\x81\x00\x33\xb1\xd0\x31\x04"
"\x17\x67\xca\x54\x72\xbc\xb7\x73\x5a\x8f\x1b\x23\x25\x7d\xcb\x23"
"\xae\x1b\x9b\xd2\x92\x80\x09\x5d\x20\x24\xd2\x73\x6f\xe7\x5a\xaf"
"\x9e\xd0\xdd\x50\x61\x96\xbf\x7c\x2d\xa1\x0a\xc4\x88\xf7\xe0\xc6"
"\xc3\x04\x35\x6f\xac\xd5\xd1\xfd\x55\xab\x6c\x99\xc7\x66\x72\xb8"
"\x70\x22\xcb\xd3\x8c\xa7\x18\x17\x2e\x25\x2f\x33\x5c\x57\x82\x67"
"\x0e\x29\xeb\x81\x74\xd3\xa3\x54\xfa\x08\xba\x87\x50\x18\xab\xc5"
"\x15\x69\xce\x4a\x73\x3b\xee\x12\x4d\x1c\x63\x11\x9b\xdf\x4d\xa1"
"\x38\x0d\xb6\x1d\xfb\xd6\xb8\x5b\xc2\x10\xd9";

static const uint8_t SSL_CERT_RSA[] =    // rsa �in input.key �inform PEM �out output.key �outform DER
"\x30\x82\x02\x5b\x02\x01\x00\x02\x81\x81\x00\xc5\xe3\x3f\x2d\x8f"
"\x98\xc2\x2a\xef\x71\xea\x40\x21\x54\x3f\x08\x62\x9c\x7b\x39\x22"
"\xfd\xda\x80\x1f\x21\x3e\x8d\x68\xcf\x8e\x6b\x70\x98\x95\x2c\x1e"
"\x4e\x79\x39\x45\xf5\xa3\xd9\x20\x54\x85\x79\x36\xf5\x08\xbe\xa0"
"\xa6\x03\x80\x60\x21\xd6\xbc\xde\xf8\xed\xe8\x73\x02\x96\x84\xcb"
"\xb4\xff\x72\x89\xf4\x56\x41\xf6\x28\xf6\x6b\x9f\x0c\x1d\xe0\x9b"
"\x21\xcb\x86\x08\xdf\x6b\xc1\x8a\xd6\xa3\x52\x2f\xfa\xd8\x5a\x2c"
"\x86\x52\x0d\x75\x2d\xf6\x17\x11\xa7\x17\xad\xc2\x3b\xd8\x0f\xcf"
"\xb7\x2b\x2c\x8a\xc4\xcd\x2d\x94\xe4\x15\x75\x02\x03\x01\x00\x01"
"\x02\x81\x80\x59\x45\x5c\x11\xf4\xae\xc8\x21\x50\x65\xc6\x74\x69"
"\xd4\xb4\x9e\xd6\xc5\x9a\xfd\x3a\xa0\xe4\x7a\x5a\x10\xc8\x44\x48"
"\xdd\x21\x75\xac\x94\xd8\xee\xcf\x39\x3d\x8c\xad\xd7\xd3\xb3\xb6"
"\xd7\x0a\x63\x95\x7c\x53\x16\x94\x28\x70\x79\xf0\x64\x33\x98\x7e"
"\xca\x33\xa0\x97\x38\x01\xe9\x06\x9b\x5c\x15\x3d\x89\xa3\x40\x2a"
"\x54\xb1\x79\x15\xf1\x7c\xfd\x18\xca\xdf\x53\x42\x6c\x8a\x0b\xc1"
"\x18\x70\xea\x7e\x00\x64\x07\x84\x37\xf2\x1b\xf5\x2a\x22\xe9\xd6"
"\xfa\x03\xc6\x7f\xaa\xc8\xa2\xa3\x67\x2a\xd3\xdd\xae\x36\x47\xc1"
"\x4f\x13\xe1\x02\x41\x00\xec\x61\x11\xbf\xcd\x87\x03\xa6\x87\xc9"
"\x2f\x1d\x80\xc1\x73\x5f\x19\xe7\x7c\xb9\x67\x7e\x49\x58\xbf\xab"
"\xd8\x37\x29\x22\x69\x79\xa4\x06\xcd\xac\x5f\x9e\xba\x12\x77\xf8"
"\x3e\xd2\x6a\x06\xb5\x90\xe4\xfa\x23\x86\xff\x41\x1b\x10\xbe\xe4"
"\x9d\x29\x75\x7c\xe6\x49\x02\x41\x00\xd6\x50\x40\xfc\xc9\x49\xad"
"\x69\x55\xc7\xa3\x5d\x51\x05\x5b\x41\x2b\xd2\x5a\x74\xf8\x15\x49"
"\x06\xf0\x1a\x6f\x7d\xb6\x65\x17\xa0\x64\xff\x7a\xd6\x99\x54\x0d"
"\x53\x95\x9f\x6c\x43\xde\x27\x1b\xe9\x24\x13\x43\xd5\xda\x22\x85"
"\x1d\xa7\x55\xa5\x4d\x0f\x5e\x45\xcd\x02\x40\x51\x92\x4d\xe5\xba"
"\xaf\x54\xfb\x2a\xf0\xaa\x69\xab\xfd\x16\x2b\x43\x6d\x37\x05\x64"
"\x49\x98\x56\x20\x0e\xd5\x56\x73\xc3\x84\x52\x8d\xe0\x2b\x29\xc8"
"\xf5\xa5\x90\xaa\x05\xe8\xe8\x03\xde\xbc\xd9\x7b\xab\x36\x87\x67"
"\x9e\xb8\x10\x57\x4f\xdd\x4c\x69\x56\xe8\xc1\x02\x40\x27\x02\x5a"
"\xa1\xe8\x9d\xa1\x93\xef\xca\x33\xe1\x33\x73\x2f\x26\x10\xac\xec"
"\x4c\x28\x2f\xef\xa7\xf4\xa2\x4b\x32\xed\xb5\x3e\xf4\xb2\x0d\x92"
"\xb5\x67\x19\x56\x87\xa5\x4f\x6c\x6c\x7a\x0e\x52\x55\x40\x7c\xc5"
"\x37\x32\xca\x5f\xc2\x83\x07\xe2\xdb\xc0\xf5\x5e\xed\x02\x40\x1b"
"\x88\xf3\x29\x8d\x6b\xdb\x39\x4c\xa6\x96\x6a\xd7\x6b\x35\x85\xde"
"\x1c\x2c\x3f\x0c\x8d\xff\xf5\xc1\xeb\x25\x3c\x56\x63\xaa\x03\xe3"
"\x10\x24\x87\x98\xd4\x73\x62\x4a\x51\x3b\x01\x9a\xda\x73\xf2\xcd"
"\xd6\xbb\xe3\x3e\x37\xb3\x19\xd9\x82\x91\x07\xdf\xd0\xa9\x80";


namespace FESL {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
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

		gettimeofday(&m_server_start, NULL);

		mp_mutex = OS::CreateMutex();
		mp_thread = OS::CreateThread(Driver::TaskThread, this, true);

		makeNonBlocking(m_sd);


		m_ssl_ctx = SSL_CTX_new(SSLv3_method());

		SSL_CTX_set_cipher_list(m_ssl_ctx, "ALL");
		SSL_CTX_set_options(m_ssl_ctx, SSL_OP_ALL);

		if (!SSL_CTX_use_certificate_ASN1(m_ssl_ctx, sizeof(SSL_CERT_X509) - 1, SSL_CERT_X509) ||
			!SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_RSA, m_ssl_ctx, SSL_CERT_RSA, sizeof(SSL_CERT_RSA) - 1)) {
			fprintf(stderr, "\nError: problems with the loading of the certificate in memory\n");
			exit(1);
		}
		SSL_CTX_set_verify_depth(m_ssl_ctx, 1);
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
		SSL_CTX_free(m_ssl_ctx);
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