#include <stdio.h>
#include <stdlib.h>

#include "QRServer.h"


#include "QRPeer.h"
#include "QRDriver.h"
#include <OS/socketlib/socketlib.h>
#include "V1Peer.h"

#include <OS/legacy/buffwriter.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/enctypex_decoder.h>
#include <OS/socketlib/socketlib.h>
#include <OS/legacy/helpers.h>

namespace QR {
	V1Peer::V1Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : Peer(driver,address_info, sd) {

	}
	V1Peer::~V1Peer() {

	}
	
	void V1Peer::think(bool packet_waiting) {

	}

	void V1Peer::handle_packet(char *recvbuf, int len) {

	}

	void V1Peer::send_error(bool die, const char *fmt, ...) {

	}
	void V1Peer::SendClientMessage(uint8_t *data, int data_len) {

	}
}