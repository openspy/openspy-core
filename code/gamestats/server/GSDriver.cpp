#include "GSDriver.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "GSServer.h"
#include "GSPeer.h"

#include <OS/GPShared.h>

namespace GS {
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : TCPDriver(server, host, port) {
	}
	Peer *Driver::FindPeerByProfileID(int profileid) {
		Peer* peer = (Peer *)GetPeerList()->GetHead();
		if (peer != NULL) {
			do {
				if (peer->GetProfileID() == profileid) {
					return peer;
				}
			} while ((peer = (Peer*)peer->GetNext()) != NULL);
		}
		return NULL;
	}
	INetPeer *Driver::CreatePeer(uv_tcp_t *sd) {
		return new Peer(this, sd);
	}
}
