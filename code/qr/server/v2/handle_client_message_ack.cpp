
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <qr/tasks/tasks.h>
#include <tasks/tasks.h>
#include <server/v2.h>
namespace QR {
	void Driver::send_client_message(int version, OS::Address to_address, uint32_t instance_key, uint32_t message_key, OS::Buffer &message_buffer) {
		OS::Buffer buffer;

		OS::LogText(OS::ELogLevel_Info, "[%s] Send client message: %d - %d", to_address.ToString().c_str(), message_buffer.bytesWritten(), message_key);

		if(version == 2) {
			buffer.WriteByte(QR_MAGIC_1);
			buffer.WriteByte(QR_MAGIC_2);

			buffer.WriteByte(PACKET_CLIENT_MESSAGE);
			buffer.WriteInt(instance_key);

			buffer.WriteInt(message_key);
			buffer.WriteBuffer(message_buffer.GetHead(), message_buffer.bytesWritten());
			SendUDPPacket(to_address, buffer);
			
		}
	}
	void Driver::handle_v2_client_message_ack(OS::Address from_address, uint8_t *instance_key, OS::Buffer &buffer) {
		uint32_t key = buffer.ReadInt();

		MM::MMPushRequest req;

		req.callback = NULL;
		req.from_address = from_address;
		req.v2_instance_key = *(uint32_t *)instance_key;
		req.driver = this;
		req.version = 2;
		req.server.id = key;

		OS::LogText(OS::ELogLevel_Info, "[%s] Got client message ack %d", from_address.ToString().c_str(), key);


		req.type = MM::EMMPushRequestType_ClientMessageAck;
		AddRequest(req);
	}
}