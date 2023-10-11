#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

#include <uv.h>

INetPeer::INetPeer(INetDriver* driver, uv_tcp_t *sd) : OS::Ref(), OS::LinkedList<INetPeer *>()
{ 
    mp_driver = driver; 
    //m_address = m_sd->address; 
    m_delete_flag = false; 
    m_timeout_flag = false; 
    m_socket_deleted = false;

    uv_async_init(uv_default_loop(), &m_async_send_handle, clear_send_buffer);
    uv_handle_set_data((uv_handle_t *)&m_async_send_handle, this);
    uv_mutex_init(&m_send_mutex);

    uv_tcp_init(uv_default_loop(), &m_socket);
    uv_accept((uv_stream_t*)sd, (uv_stream_t *)&m_socket);
    uv_handle_set_data((uv_handle_t *)&m_socket, this);
    uv_read_start((uv_stream_t *)&m_socket, INetPeer::read_alloc_cb, INetPeer::stream_read);
}
void INetPeer::stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    INetPeer *peer = (INetPeer*)uv_handle_get_data((const uv_handle_t*)stream);
    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        }
        peer->Delete();
    } else if (nread > 0) {        
        peer->on_stream_read(stream, nread, buf);
    }

    if (buf->base) {
        free(buf->base);
    }
}

	void INetPeer::write_callback(uv_write_t *req, int status) {
        UVWriteData *write_data = (UVWriteData *)uv_handle_get_data((uv_handle_t*) req);
        delete write_data;
	}


	void INetPeer::clear_send_buffer(uv_async_t *handle) {
		//std::vector<OS::Buffer> m_send_buffer;
		INetPeer *peer = (INetPeer *)uv_handle_get_data((uv_handle_t*)handle);
		if(peer->m_delete_flag) {
			return;
		}

		uv_mutex_lock(&peer->m_send_mutex);
		
		while(!peer->m_send_buffer.empty()) {
			OS::Buffer send_buffer = peer->m_send_buffer.top();

			uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));

			UVWriteData *write_data = new UVWriteData();
			write_data->send_buffer.WriteBuffer(send_buffer.GetHead(), send_buffer.bytesWritten());
			write_data->uv_buffer = uv_buf_init((char *)write_data->send_buffer.GetHead(), write_data->send_buffer.bytesWritten());
			uv_handle_set_data((uv_handle_t*) req, write_data);

			int r = uv_write(req, (uv_stream_t*)&peer->m_socket, &write_data->uv_buffer, 1, write_callback);

			if (r < 0) {
				OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d", peer->getAddress().ToString().c_str(), r);
			}

			peer->m_send_buffer.pop();
		}
		uv_mutex_unlock(&peer->m_send_mutex);
	}
    void INetPeer::append_send_buffer(OS::Buffer buffer) {
		uv_mutex_lock(&m_send_mutex);
		m_send_buffer.push(buffer);
		uv_mutex_unlock(&m_send_mutex);

		uv_async_send(&m_async_send_handle);
    }