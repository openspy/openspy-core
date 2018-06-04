#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/StringCrypter.h>
#include <OS/Net/NetDriver.h>
#include <OS/Net/IOIfaces/SSLIOInterface.h>
#include "FESLPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

#define DRIVER_THREAD_TIME 1000

namespace FESL {


	typedef struct {
		std::string domainPartition;
		std::string subDomain;
		std::string messagingHostname;
		uint16_t  messagingPort;
		std::string theaterHostname;
		uint16_t  theaterPort;
	} PublicInfo;


	class Peer;

	class Driver : public INetDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, PublicInfo public_info, std::string str_crypter_rsa_key, bool use_ssl = true, const char *x509_path = NULL, const char *rsa_priv_path = NULL, SSLNetIOIFace::ESSL_Type ssl_version = SSLNetIOIFace::ESSL_SSLv3);
		~Driver();
		void think(bool listener_waiting);

		INetIOSocket *getListenerSocket() const;
		const std::vector<INetIOSocket *> getSockets() const;

		const std::vector<INetPeer *> getPeers(bool inc_ref = false);

		PublicInfo GetServerInfo() { return m_server_info; };

		SSLNetIOIFace::SSLNetIOInterface *getSSL_Socket_Interface();

		OS::StringCrypter *getStringCrypter() const { return mp_string_crypter; };

		void OnUserAuth(OS::Address remote_address, int userid, int profileid);
	private:
		static void *TaskThread(OS::CThread *thread);
		void TickConnections();

		std::vector<FESL::Peer *> m_peers_to_delete;

		std::vector<Peer *> m_connections;

		OS::CMutex *mp_mutex;
		OS::CThread *mp_thread;

		RSA *m_encrypted_login_info_key;

		PublicInfo m_server_info;

		INetIOSocket *mp_socket;

		OS::StringCrypter *mp_string_crypter;

		SSLNetIOIFace::SSLNetIOInterface *mp_socket_interface;
	};
}
#endif //_SBDRIVER_H