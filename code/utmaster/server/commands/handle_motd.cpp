#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::send_motd() {
		OS::Buffer send_buffer;

		//Write_FString(m_config->motd, send_buffer);

		send_buffer.WriteByte(0x6A);
		send_buffer.WriteByte(0x16);
		send_buffer.WriteNTS(m_config->motd);
		
		send_buffer.WriteInt(0); //??
		send_packet(send_buffer);
	}

}