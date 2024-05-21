#include <stdio.h>
#include <stdlib.h>
#include  <algorithm>
#include "QRServer.h"
#include "QRDriver.h"
#include "v2.h"
namespace QR {
    void Driver::handle_v2_packet(OS::Address address, OS::Buffer buffer) {
		uint8_t type = buffer.ReadByte();
		if(buffer.readRemaining() < REQUEST_KEY_LEN) {
			return;
		}

		uint8_t instance_key[REQUEST_KEY_LEN];
		buffer.ReadBuffer(&instance_key, REQUEST_KEY_LEN);

		OS::LogText(OS::ELogLevel_Info, "[%s] Got request %d with instance key: %d", address.ToString().c_str(), *(uint32_t*)&instance_key[0])
		switch(type) {
			case PACKET_AVAILABLE:
				handle_v2_available(address, (uint8_t *)&instance_key, buffer);
				break;
			case PACKET_HEARTBEAT:
				handle_v2_heartbeat(address, (uint8_t *)&instance_key, buffer);
			break;
			case PACKET_CHALLENGE:
				handle_v2_challenge(address, (uint8_t *)&instance_key, buffer);
			break;
			case PACKET_KEEPALIVE:
				handle_v2_keepalive(address, (uint8_t *)&instance_key, buffer);
			break;
			case PACKET_CLIENT_MESSAGE_ACK:
				handle_v2_client_message_ack(address, (uint8_t *)&instance_key, buffer);
			break;
			default:
				OS::LogText(OS::ELogLevel_Info, "[%s] Got Unknown QR2 packet %d", address.ToString().c_str(), type);
			break;
		}

    }
}