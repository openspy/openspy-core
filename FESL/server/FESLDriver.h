#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/StringCrypter.h>
#include <OS/Net/drivers/TCPDriver.h>
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
		std::string termsOfServiceData;
		int gameid; //OpenSpy gameid
	} PublicInfo;


	class Peer;

	class Driver : public TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, PublicInfo public_info, std::string str_crypter_rsa_key, const char *x509_path = NULL, const char *rsa_priv_path = NULL, SSLNetIOIFace::ESSL_Type ssl_version = SSLNetIOIFace::ESSL_None, bool proxyFlag = false);
		~Driver();

		PublicInfo GetServerInfo() { return m_server_info; };

		OS::StringCrypter *getStringCrypter() const { return mp_string_crypter; };

		void OnUserAuth(std::string session_key, int userid, int profileid);

		INetPeer *CreatePeer(INetIOSocket *socket);
	private:
		RSA *m_encrypted_login_info_key;

		PublicInfo m_server_info;

		OS::StringCrypter *mp_string_crypter;
	};
}
#endif //_SBDRIVER_H