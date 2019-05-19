#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <OS/SharedTasks/tasks.h>
#include "Driver.h"
namespace Peerchat {
    Driver::Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {

    }
    Peer *Driver::FindPeerByProfileID(int profileid) {
        return NULL;
    }
    Peer *Driver::FindPeerByUserSummary(std::string summary_string) {
		mp_mutex->lock();
		std::vector<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *peer = (Peer *)*it;
			if (stricmp(peer->GetUserDetails().ToString().c_str(), summary_string.c_str()) == 0 && !peer->ShouldDelete()) {
				mp_mutex->unlock();
				return peer;
			}
			it++;
		}
		mp_mutex->unlock();
		return NULL;   
    }
    INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
        return new Peer(this, socket);
    }
}