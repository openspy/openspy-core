#ifndef _NETPEER_H
#define _NETPEER_H
#include <OS/Ref.h>
#include "NetDriver.h"
#include "NetIOInterface.h"
#include <OS/Net/NetServer.h>
#include <OS/LinkedList.h>
class INetPeer : public OS::Ref, public OS::LinkedList<INetPeer *> {
	public:
		INetPeer(INetDriver* driver, INetIOSocket* sd) : OS::Ref(), OS::LinkedList<INetPeer *>()
		{ 
			mp_driver = driver; 
			m_sd = sd;  
			m_address = m_sd->address; 
			m_delete_flag = false; 
			m_timeout_flag = false; 
			m_socket_deleted = false;
		}
		virtual ~INetPeer() { 
			CloseSocket();
		}

		void SetAddress(OS::Address address) { m_address = address; }
		virtual void OnConnectionReady() = 0;
		virtual void think(bool packet_waiting) = 0;

		INetIOSocket *GetSocket() { return m_sd; };
		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		INetDriver *GetDriver() { return mp_driver; };

		OS::Address getAddress() { return m_address; };

		virtual void Delete(bool timeout = false) = 0;

		void CloseSocket() {
            if(!m_socket_deleted) {
                m_socket_deleted = true;
                GetDriver()->getNetIOInterface()->closeSocket(m_sd);
            }
		}
	protected:
		INetIOSocket *m_sd;
		INetDriver *mp_driver;
		struct timeval m_last_recv, m_last_ping;
		OS::Address m_address;

		bool m_socket_deleted;
		bool m_delete_flag;
		bool m_timeout_flag;
	};
#endif
