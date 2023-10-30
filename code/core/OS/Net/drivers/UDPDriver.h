#ifndef _UDPDRIVER_H
#define _UDPDRIVER_H

#include <OS/Net/NetPeer.h>
#include <OS/KVReader.h>

namespace OS {
	class UDPDriver : public INetDriver {
		public:
			UDPDriver(INetServer *server, const char *host, uint16_t port);
			virtual ~UDPDriver();
			void think();
			void SendUDPPacket(OS::Address to, OS::Buffer buffer);
	        static void on_udp_send_callback(uv_udp_send_t* req, int status);
		protected:
            static void clear_send_buffer(uv_async_t *handle);

            uv_udp_t m_recv_udp_socket;
            uv_async_t m_udp_send_async_handler;
            std::stack<std::pair<OS::Address, OS::Buffer>> m_udp_send_buffer;
            uv_mutex_t m_udp_send_mutex;

	};
}
#endif //_UDPDRIVER_H