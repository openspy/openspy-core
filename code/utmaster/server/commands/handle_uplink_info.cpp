#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::handle_uplink_info(OS::Buffer buffer) {
		//send_heartbeat_request(0, 1111);
		//send_heartbeat_request(1, 1111);
		//send_heartbeat_request(2, 1111);

		send_detected_ports();

		uint32_t behind_nat = buffer.ReadInt(), gamespy_uplink = buffer.ReadInt();

		OS::LogText(OS::ELogLevel_Info, "[%s] Behind NAT: %d, GameSpy Uplink: %d", getAddress().ToString().c_str(), behind_nat, gamespy_uplink);
	}
	void Peer::send_detected_ports() {
		//TODO: make this actually work

		uint16_t game_port = 7777;

		OS::Buffer send_data;
		send_data.WriteByte(EServerOutgoingRequest_DetectedPorts);

		if (m_client_version >= 3000) {
			send_data.WriteInt(120); //???

			//ports are int32? maybe there is extra data here
			send_data.WriteInt(game_port + 1); //7778
			send_data.WriteInt(game_port); //7777
			send_data.WriteInt(game_port + 10); //7787
		}
		else { //UT2003 - this may not even be what it is
			send_data.WriteByte(0); //???

			//ports are int32? maybe there is extra data here
			send_data.WriteInt(game_port); //7777
		}

		send_packet(send_data);
	}
}