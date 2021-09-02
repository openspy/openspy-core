#ifndef _GPDRIVER_H
#define _GPDRIVER_H
#include <stdint.h>
#include "../main.h"
#include <OS/Net/drivers/TCPDriver.h>

#include "UTPeer.h"

#include <map>
#include <vector>
#ifdef _WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace UT {
	class Peer;

	class Config {
		public:
			Config() {
				gameid = 0;
				is_server = 0;
			}
			std::string clientName;
			int gameid;
			std::string motd;
			bool is_server;
	};

	class Driver : public TCPDriver {
	public:
		Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders = false);
		void SetConfig(std::vector<UT::Config *> config) { m_config = config; }
		UT::Config *FindConfigByClientName(std::string clientName);
	protected:
		virtual INetPeer *CreatePeer(INetIOSocket *socket);
		std::vector<UT::Config *> m_config;
	};
}
#endif //_SBDRIVER_H