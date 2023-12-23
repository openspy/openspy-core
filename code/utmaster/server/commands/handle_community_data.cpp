#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
	void Peer::send_community_motd() {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(0);
		Write_FString(m_config->community_motd, send_buffer);
		send_packet(send_buffer);

	}
	void Peer::send_community_map(UT::MapItem data) {
		OS::Buffer send_buffer;
		send_buffer.WriteByte(1);
		send_buffer.WriteInt(0);
		send_buffer.WriteInt(data.version);
		Write_FString(data.name, send_buffer);

		send_buffer.WriteByte(1);
		send_buffer.WriteInt(1);
		send_buffer.WriteInt(data.version);
		Write_FString(data.description, send_buffer);

		send_buffer.WriteByte(1);
		send_buffer.WriteInt(2);
		send_buffer.WriteInt(data.version);
		Write_FString(data.url, send_buffer);

		send_packet(send_buffer);
	}
	void Peer::handle_community_data(OS::Buffer recv_buffer) {
		if (m_config->is_server) return;
		send_community_motd();

		int version = recv_buffer.ReadInt();

		std::vector<UT::MapItem>::iterator it = m_config->maps.begin();
		while (it != m_config->maps.end()) {
			if (it->version > version) {
				send_community_map(*it);
			}
			it++;
		}

		OS::Buffer send_buffer;
		send_buffer.WriteByte(2);
		send_packet(send_buffer);

		Delete();
	}

}