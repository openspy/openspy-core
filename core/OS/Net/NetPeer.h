#ifndef _NETPEER_H
#define _NETPEER_H
#include <OS/Ref.h>
#include "NetDriver.h"
class INetPeer : public OS::Ref {
	public:
		INetPeer(INetDriver *driver, struct sockaddr_in *address_info, int sd) : OS::Ref() { mp_driver = driver; m_address_info = *address_info; m_sd = sd; };
		~INetPeer() { if (m_sd != mp_driver->getListenerSocket()) { close(m_sd); } }

		virtual void think(bool packet_waiting) = 0;
		const struct sockaddr_in *getAddress() { return &m_address_info; }

		int GetSocket() { return m_sd; };
		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		INetDriver *GetDriver() { return mp_driver; };
	protected:
		int m_sd;
		INetDriver *mp_driver;
		struct sockaddr_in m_address_info;
		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		OS::CMutex *mp_mutex;
	private:

	};
#endif