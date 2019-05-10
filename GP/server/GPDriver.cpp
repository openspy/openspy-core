#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <OS/SharedTasks/tasks.h>
#include "GPDriver.h"
#include "GPServer.h"
#include "GPPeer.h"
#include <OS/GPShared.h>

namespace GP {
	Driver::Driver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders) : TCPDriver(server, host, port, proxyHeaders) {
	}

	Peer *Driver::FindPeerByProfileID(int profileid) {
		std::deque<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = (Peer *)*it;
			if(p == NULL) break;
			if(!p->ShouldDelete() && p->GetProfileID() == profileid) {
				return p;
			}
			it++;
		}
		return NULL;
	}
	void Driver::InformStatusUpdate(int from_profileid, GPShared::GPStatus status) {
		std::deque<INetPeer *>::iterator it = m_connections.begin();
		while (it != m_connections.end()) {
			Peer *p = (Peer *)*it;
			if(p == NULL) break;
			if(!p->ShouldDelete())
				p->inform_status_update(from_profileid, status);
			it++;
		}
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		return new Peer(this, socket);
	}
}
