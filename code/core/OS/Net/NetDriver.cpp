#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

INetDriver::INetDriver(INetServer *server) {
	uv_mutex_init(&m_udp_send_mutex);
	uv_async_init(uv_default_loop(), &m_udp_send_async_handler, clear_send_buffer);
	uv_handle_set_data((uv_handle_t *)&m_udp_send_async_handler, this);
    mp_peers = new OS::LinkedListHead<INetPeer*>();
	m_server = server;	
}
INetDriver::~INetDriver() {
	delete mp_peers;
}
void INetDriver::SendUDPPacket(OS::Address to, OS::Buffer buffer) {
	//hopefully this is performant... need to figure out a way that doesn't use a mutex to send
	std::pair<OS::Address, OS::Buffer> pair(to, buffer);
	uv_mutex_lock(&m_udp_send_mutex);
	m_udp_send_buffer.push(pair);
	uv_mutex_unlock(&m_udp_send_mutex);
	uv_async_send(&m_udp_send_async_handler);
	

}
void INetDriver::on_udp_send_callback(uv_udp_send_t* req, int status) {
	OS::Buffer *buffer = (OS::Buffer *)uv_handle_get_data((uv_handle_t*) req);
	delete buffer;
	free((void *)req);
}

void INetDriver::clear_send_buffer(uv_async_t *handle) {
		//std::vector<OS::Buffer> m_send_buffer;
		INetDriver *driver = (INetDriver *)uv_handle_get_data((uv_handle_t*)handle);

		uv_mutex_lock(&driver->m_udp_send_mutex);
		
		while(!driver->m_udp_send_buffer.empty()) {
			std::pair<OS::Address, OS::Buffer> send_data = driver->m_udp_send_buffer.top();

			uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t));
			struct sockaddr_in addr = send_data.first.GetInAddr();

			OS::Buffer *copy_buffer = new OS::Buffer(send_data.second);
			uv_handle_set_data((uv_handle_t*) req, copy_buffer);

			uv_buf_t buf = uv_buf_init((char *)copy_buffer->GetHead(), copy_buffer->bytesWritten());

			int r = uv_udp_send(req,
				&driver->m_recv_udp_socket,
				&buf,
				1,
				(const struct sockaddr*) &addr,
				INetDriver::on_udp_send_callback);

			if (r < 0) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d", send_data.first.ToString().c_str(), r);
			}

			driver->m_udp_send_buffer.pop();
		}
		uv_mutex_unlock(&driver->m_udp_send_mutex);
}