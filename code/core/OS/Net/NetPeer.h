#ifndef _NETPEER_H
#define _NETPEER_H
#include <OS/Ref.h>
#include "NetDriver.h"
#include "NetIOInterface.h"
#include <OS/Net/NetServer.h>
#include <OS/LinkedList.h>

#include <uv.h>
class INetPeer : public OS::Ref, public OS::LinkedList<INetPeer *> {
	public:
		INetPeer(INetDriver* driver, uv_tcp_t *sd) : OS::Ref(), OS::LinkedList<INetPeer *>()
		{ 
			printf("NetPeer\n");
			mp_driver = driver; 
			//m_address = m_sd->address; 
			m_delete_flag = false; 
			m_timeout_flag = false; 
			m_socket_deleted = false;

			uv_tcp_init(uv_default_loop(), &m_socket);
			uv_accept((uv_stream_t*)sd, (uv_stream_t *)&m_socket);
			uv_handle_set_data((uv_handle_t *)&m_socket, this);
			uv_read_start((uv_stream_t *)&m_socket, INetPeer::read_alloc_cb, INetPeer::stream_read);
		}
		virtual ~INetPeer() { 
			CloseSocket();
		}

		void SetAddress(OS::Address address) { m_address = address; }
		virtual void OnConnectionReady() = 0;
		virtual void think(bool packet_waiting) = 0;

		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		INetDriver *GetDriver() { return mp_driver; };

		OS::Address getAddress() { return m_address; };

		virtual void Delete(bool timeout = false) = 0;

		static void stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
		virtual void on_stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) = 0;

		void CloseSocket() {
            if(!m_socket_deleted) {
                m_socket_deleted = true;
                uv_close((uv_handle_t*)&m_socket, NULL);
            }
		}
	protected:
		static void read_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
			buf->base = (char*)malloc(suggested_size);
			buf->len = suggested_size;
		}

		uv_tcp_t m_socket;
		INetDriver *mp_driver;
		struct timeval m_last_recv, m_last_ping;
		OS::Address m_address;

		bool m_socket_deleted;
		bool m_delete_flag;
		bool m_timeout_flag;
	};
#endif
