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
		Peer* peer = (Peer*)GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				if (peer->GetProfileID() == profileid) {
					return peer;
				}
			} while ((peer = (Peer*)peer->GetNext()) != NULL);
		}
		return NULL;
	}
	void Driver::InformStatusUpdate(int from_profileid, GPShared::GPStatus status) {
		Peer* peer = (Peer*)GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				peer->inform_status_update(from_profileid, status);
			} while ((peer = (Peer*)peer->GetNext()) != NULL);
		}
	}
	INetPeer *Driver::CreatePeer(INetIOSocket *socket) {
		return new Peer(this, socket);
	}
}
