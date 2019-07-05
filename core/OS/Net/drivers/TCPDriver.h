#ifndef _TCPDRIVER_H
#define _TCPDRIVER_H

#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

#include <OS/Net/IOIfaces/SSLIOInterface.h>

#define TCP_PING_TIME (60)
#define DRIVER_THREAD_TIME 1000
#define CONNECTIONS_RESERVE_SIZE 1024

class TCPDriver : public INetDriver {
    public:
		TCPDriver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders = false, const char *x509_path = NULL, const char *rsa_priv_path = NULL, SSLNetIOIFace::ESSL_Type ssl_version = SSLNetIOIFace::ESSL_None);
		~TCPDriver();
		void think(bool packet_waiting);

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);
		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;
		void OnPeerMessage(INetPeer *peer);
    protected:
		virtual INetPeer *CreatePeer(INetIOSocket *socket) = 0;
		static void *TaskThread(OS::CThread *thread);
		virtual void TickConnections();
		void DeleteClients();

		std::vector<INetPeer *> m_connections;

		struct timeval m_server_start;

		std::vector<INetPeer *> m_peers_to_delete;
		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		INetIOSocket *mp_socket;

		bool m_proxy_headers;
};
#endif //_TCPDRIVER_H