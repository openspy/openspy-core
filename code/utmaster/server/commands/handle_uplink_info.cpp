#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::handle_uplink_info(OS::Buffer buffer) {
		send_heartbeat_request(0, 1111);
		send_heartbeat_request(1, 1111);
		send_heartbeat_request(2, 1111);
		send_inform_create_server();

		uint32_t behind_nat = buffer.ReadInt(), gamespy_uplink = buffer.ReadInt();

		OS::LogText(OS::ELogLevel_Info, "[%s] Behind NAT: %d, GameSpy Uplink: %d", getAddress().ToString().c_str(), behind_nat, gamespy_uplink);
	}
	void Peer::send_inform_create_server() {
		char unknown_data[] = {
			0x78, 0x00, 0x00, 0x00, 0x62, 0x1e, 0x00, 0x00, 0x61, 0x1e, 0x00, 0x00, 0x6b, 0x1e, 0x00, 0x00
		};

		OS::Buffer send_data;
		send_data.WriteByte(EServerOutgoingRequest_InformCreateServer);
		send_data.WriteBuffer((const void*)unknown_data, sizeof(unknown_data));
		send_packet(send_data);
	}
}