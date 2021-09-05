#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <sstream>

#include "../UTPeer.h"
#include "../UTDriver.h"
#include "../UTServer.h"
#include <tasks/tasks.h>

namespace UT {
    void Peer::handle_server_init(OS::Buffer recv_buffer) {
            if(m_got_server_init) return;
			OS::Buffer send_buffer;

            send_buffer.WriteByte(1);	
            send_buffer.WriteByte(0);
            send_buffer.WriteInt(1234);

			send_packet(send_buffer);

            m_got_server_init = true;
    }
}
