#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <sstream>
#include "QRServer.h"
#include "QRDriver.h"

#include "v2.h"


namespace QR {
	void alloc_buffer(uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t* buf) {
		buf->base = (char *)malloc(suggested_size);
		buf->len = suggested_size;
	}
	Driver::Driver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
		uv_udp_init(uv_default_loop(), &m_recv_udp_socket);
		uv_handle_set_data((uv_handle_t*) &m_recv_udp_socket, this);
		
    	uv_ip4_addr(host, port, &m_recv_addr);

    	uv_udp_bind(&m_recv_udp_socket, (const struct sockaddr *)&m_recv_addr, UV_UDP_REUSEADDR);
    	uv_udp_recv_start(&m_recv_udp_socket, alloc_buffer, Driver::on_udp_read);

		gettimeofday(&m_server_start, NULL);
	}
	Driver::~Driver() {
		uv_close((uv_handle_t *)&m_recv_udp_socket, NULL); //XXX: this should actually be moved to not be in the ctor..
	}
	void Driver::think(bool listener_waiting) {
	}

	void Driver::on_udp_read(uv_udp_t* handle,
                               ssize_t nread,
                               const uv_buf_t* buf,
                               const struct sockaddr* addr,
                               unsigned flags) {
		Driver *driver = (Driver *)uv_handle_get_data((uv_handle_t*) handle);
		if(nread > 0) {
			OS::Buffer buffer;
			buffer.WriteBuffer(buf->base, nread);
			buffer.resetReadCursor();

			OS::Address address = *(struct sockaddr_in *)addr;

			if(buf->base[0] == '\\') {
				driver->handle_v1_packet(address, buffer);
			} else {
				driver->handle_v2_packet(address, buffer);
			}
		}
	}


	void Driver::send_v2_error(OS::Address to, uint32_t instance_key, uint8_t error_code, const char *error_message) {
		OS::Buffer buffer;		
		buffer.WriteByte(QR_MAGIC_1);
		buffer.WriteByte(QR_MAGIC_2);
		buffer.WriteByte(PACKET_ADDERROR);
		buffer.WriteInt(instance_key);
		buffer.WriteByte(error_code); //error code
		buffer.WriteNTS(error_message);
		SendUDPPacket(to, buffer);

		OS::LogText(OS::ELogLevel_Info, "[%s] Send Error: %s", to.ToString().c_str(), error_message);
	}

	void Driver::send_v1_error(OS::Address to, const char *error_message) {
		return;
		std::stringstream ss;
		ss << "\\error\\" << error_message;

		std::string message = ss.str();

		OS::LogText(OS::ELogLevel_Info, "[%s] Send Error: %s", to.ToString().c_str(), error_message);

		OS::Buffer buffer;
		buffer.WriteBuffer(message.c_str(),message.length());
		SendUDPPacket(to, buffer);
	}

	void Driver::AddRequest(MM::MMPushRequest req) {
		uv_work_t *uv_req = (uv_work_t*)malloc(sizeof(uv_work_t));

		MM::MMWorkData *work_data = new MM::MMWorkData();
		work_data->request = req;

		uv_handle_set_data((uv_handle_t*) uv_req, work_data);

		uv_queue_work(uv_default_loop(), uv_req, MM::PerformUVWorkRequest, MM::PerformUVWorkRequestCleanup);
	}
}