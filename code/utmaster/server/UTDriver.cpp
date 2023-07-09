#include <stdio.h>
#include <stdlib.h>
#include "UTServer.h"
#include "UTPeer.h"
#include "UTDriver.h"

namespace UT {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		return new Peer(this, socket);
	}
	UT::Config *Driver::FindConfigByClientName(std::string clientName) {
		std::vector<UT::Config *>::iterator it = m_config.begin();
		
		while(it != m_config.end()) {
			UT::Config *config = *it;
			if(config->clientName.compare(clientName) == 0) {
				return config;
			}
			it++;
		}
		return NULL;
	}
	void Driver::SetConfig(std::vector<UT::Config*> config) { 
		m_config = config;

		/*std::vector<UT::Config*>::iterator it = config.begin();
		while (it != config.end()) {
			UT::Config* cfg = *it;

			cfg->game_data = OS::GetGameByID(cfg->gameid);
			it++;
		}*/
	}
}
