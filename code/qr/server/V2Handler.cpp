#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"
#include "QRDriver.h"
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include "v2.h"
namespace QR {
    void Driver::handle_v2_packet(INetIODatagram &packet) {
		bool continueRead = true;
		do {
			uint8_t type = packet.buffer.ReadByte();
			if(packet.buffer.readRemaining() < 5) {
				break;
			}

			uint8_t instance_key[REQUEST_KEY_LEN];
			packet.buffer.ReadBuffer(&instance_key, REQUEST_KEY_LEN);

			switch(type) {
				case PACKET_AVAILABLE:
					handle_v2_available(packet.address, (uint8_t *)&instance_key, packet.buffer);
					break;
				case PACKET_HEARTBEAT:
					handle_v2_heartbeat(packet.address, (uint8_t *)&instance_key, packet.buffer);
				break;
				case PACKET_CHALLENGE:
					handle_v2_challenge(packet.address, (uint8_t *)&instance_key, packet.buffer);
				break;
				case PACKET_KEEPALIVE:
					handle_v2_keepalive(packet.address, (uint8_t *)&instance_key, packet.buffer);
				break;
				case PACKET_CLIENT_MESSAGE_ACK:
					handle_v2_client_message_ack(packet.address, (uint8_t *)&instance_key, packet.buffer);
				break;
				default:
					continueRead = false;
				break;
			}
		} while(continueRead);

    }
}