#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <SSL/StringCrypter.h>
#include <OS/Net/drivers/TCPDriver.h>
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

	class Driver : public OS::TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, PublicInfo public_info, std::string str_crypter_rsa_key);
		~Driver();

		PublicInfo GetServerInfo() { return m_server_info; };

		OS::StringCrypter *getStringCrypter() const { return mp_string_crypter; };

		void OnUserAuth(std::string session_key, int userid, int profileid);

		INetPeer *CreatePeer(uv_tcp_t *sd);
	private:
		PublicInfo m_server_info;

		OS::StringCrypter *mp_string_crypter;
	};
}
#endif //_SBDRIVER_H