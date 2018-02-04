#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/NetDriver.h>

#include "FESLPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <openssl/ssl.h>
#define DRIVER_THREAD_TIME 1000

namespace FESL {
	enum EFESLSSL_Type {
		EFESLSSL_SSLv23,
		EFESLSSL_SSLv2,
		EFESLSSL_SSLv3,
		EFESLSSL_TLS10,
		EFESLSSL_TLS11,
		EFESLSSL_TLS12,
	};
	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, bool use_ssl = true, const char *x509_path = NULL, const char *rsa_priv_path = NULL, EFESLSSL_Type ssl_version = EFESLSSL_SSLv3);
		~Driver();
		void think(bool listener_waiting);
		int getListenerSocket();
		uint16_t getPort();
		uint32_t getBindIP();
		uint32_t getDeltaTime();

		Peer *find_client(struct sockaddr_in *address);
		Peer *find_or_create(struct sockaddr_in *address);
		bool HasPeer(Peer *);

		const std::vector<int> getSockets();
		int GetNumConnections();

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);

		OS::MetricInstance GetMetrics();

		SSL_CTX *getSSLCtx() { return m_ssl_ctx;  };
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();

		int m_sd;
		std::vector<FESL::Peer *> m_peers_to_delete;

		//safe for now, until pointers one day get added
		std::queue<PeerStats> m_stats_queue; //pending stats to be sent(deleted clients)

		std::vector<Peer *> m_connections;
		
		struct sockaddr_in m_local_addr;

		struct timeval m_server_start;

		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		SSL_CTX *m_ssl_ctx;

		void *mp_x509_cert_data;
		void *mp_rsa_key_data;
	};
}
#endif //_SBDRIVER_H