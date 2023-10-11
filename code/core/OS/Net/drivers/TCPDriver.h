#ifndef _TCPDRIVER_H
#define _TCPDRIVER_H

#include <OS/User.h>
#include <OS/Profile.h>
#include <OS/Mutex.h>

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

#include <OS/SSL.h>

#include <SSL/SSLIOInterface.h>

#define DRIVER_THREAD_TIME 1000

namespace OS {
	class TCPDriver : public INetDriver {
		public:
			TCPDriver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders = false, const char *x509_path = NULL, const char *rsa_priv_path = NULL, ESSL_Type ssl_version = ESSL_None);
			virtual ~TCPDriver();
			void think(bool packet_waiting);

			INetIOSocket *getListenerSocket() const;
			void OnPeerMessage(INetPeer *peer);

			//Linked List iterators
			static bool LLIterator_DeleteAllClients(INetPeer* peer, TCPDriver* driver);
			static bool LLIterator_TickOrDeleteClient(INetPeer* peer, TCPDriver* driver);
			//
		protected:
			virtual INetPeer *CreatePeer(uv_tcp_t *socket) = 0;
			static void *TaskThread(OS::CThread *thread);
			virtual void TickConnections();
			void DeleteClients();
			static void on_new_connection(uv_stream_t *server, int status);

			struct timeval m_server_start;

			OS::CMutex *mp_mutex;
			OS::CThread *mp_thread;

			uv_tcp_t m_listener_socket;

			bool m_proxy_headers;		
	};
}
#endif //_TCPDRIVER_H