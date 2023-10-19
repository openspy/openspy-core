#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

#include <uv.h>

INetPeer::INetPeer(INetDriver* driver, uv_tcp_t *sd) : OS::Ref(), OS::LinkedList<INetPeer *>()
{ 
    mp_driver = driver; 
    m_delete_flag = false; 
    m_timeout_flag = false; 
    m_socket_deleted = false;
    m_close_when_sendbuffer_empty = false;

    uv_async_init(uv_default_loop(), &m_async_send_handle, clear_send_buffer);
    uv_handle_set_data((uv_handle_t *)&m_async_send_handle, this);
    uv_mutex_init(&m_send_mutex);

    uv_tcp_init(uv_default_loop(), &m_socket);
    uv_handle_set_data((uv_handle_t *)&m_socket, this);

    int r = uv_accept((uv_stream_t*)sd, (uv_stream_t *)&m_socket);
    if(r < 0) {
        OS::LogText(OS::ELogLevel_Error, "Failed to accept TCP connection: %s", uv_strerror(r));
    }


    struct sockaddr_in name;
    int nlen = sizeof(name);
    r = uv_tcp_getpeername(&m_socket, (struct sockaddr *)&name, &nlen);
    if(r < 0) {
        OS::LogText(OS::ELogLevel_Error, "[%s] TCP getpeername failed: %s", m_address.ToString().c_str(), uv_strerror(r));
    }

    m_address = OS::Address(name);

    r = uv_read_start((uv_stream_t *)&m_socket, INetPeer::read_alloc_cb, INetPeer::stream_read);
    if(r < 0) {
        OS::LogText(OS::ELogLevel_Error, "[%s] Failed to start TCP reader: %s", m_address.ToString().c_str(), uv_strerror(r));
    }
}
INetPeer::~INetPeer() {
}
void INetPeer::stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    INetPeer *peer = (INetPeer*)uv_handle_get_data((const uv_handle_t*)stream);
    if (nread < 0) {
        if (nread != UV_EOF) {
            OS::LogText(OS::ELogLevel_Info, "[%s] Read error %s\n", peer->m_address.ToString().c_str(), uv_err_name(nread));
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
        write_data->peer->DecRef();
        free((void *)req);

        if(write_data->peer->m_close_when_sendbuffer_empty) {
            write_data->peer->CloseSocket();
        }
        delete write_data;
	}


	void INetPeer::clear_send_buffer(uv_async_t *handle) {
		INetPeer *peer = (INetPeer *)uv_handle_get_data((uv_handle_t*)handle);

		uv_mutex_lock(&peer->m_send_mutex);

        if(!peer->m_send_buffer.empty()) {
            
            int num_buffers = peer->m_send_buffer.size();
            UVWriteData *write_data = new UVWriteData(num_buffers, peer);
            
            uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
            uv_handle_set_data((uv_handle_t*) req, write_data);
            
            
            for(int i=0;i<num_buffers;i++) {
                OS::Buffer send_buffer = peer->m_send_buffer.front();
                write_data->send_buffers[i].WriteBuffer(send_buffer.GetHead(), send_buffer.bytesWritten());
                write_data->uv_buffers[i] =  uv_buf_init((char *)write_data->send_buffers[i].GetHead(), write_data->send_buffers[i].bytesWritten());
                peer->m_send_buffer.pop();
            }
            int r = uv_write(req, (uv_stream_t*)&peer->m_socket, write_data->uv_buffers, num_buffers, write_callback);
            if (r < 0) {
                OS::LogText(OS::ELogLevel_Info, "[%s] Got Send error - %d - %s", peer->getAddress().ToString().c_str(), r, uv_err_name(r));
            }

        }
		uv_mutex_unlock(&peer->m_send_mutex);        
	}
    void INetPeer::append_send_buffer(OS::Buffer buffer, bool close_after) {
		uv_mutex_lock(&m_send_mutex);
		m_send_buffer.push(buffer);
        IncRef();
		uv_mutex_unlock(&m_send_mutex);

        if(close_after) {
            m_close_when_sendbuffer_empty = true;
        }

		int r = uv_async_send(&m_async_send_handle);
    }
    void INetPeer::CloseSocket() {
    if(!m_socket_deleted) {
        m_socket_deleted = true;
        
        IncRef();
        uv_close((uv_handle_t*)&m_async_send_handle, close_callback);

        IncRef();
        uv_close((uv_handle_t*)&m_socket, close_callback);
    }
}
void INetPeer::close_callback(uv_handle_t *handle) {
    INetPeer *peer = (INetPeer *)uv_handle_get_data(handle);
    peer->DecRef();
    if(peer->GetRefCount() == 0) {
        delete peer;
    }
    
}
