#ifndef _NETPEER_H
#define _NETPEER_H
#include <OS/Ref.h>
#include <OS/Analytics/Metric.h>
#include "NetDriver.h"
#include "NetIOInterface.h"
#include <OS/Net/NetServer.h>
class INetPeer : public OS::Ref {
	public:
		INetPeer(INetDriver *driver, INetIOSocket *sd) : OS::Ref() { mp_driver = driver; m_sd = sd; };
		virtual ~INetPeer() { GetDriver()->getServer()->getNetIOInterface()->closeSocket(m_sd); }

		virtual void think(bool packet_waiting) = 0;

		INetIOSocket *GetSocket() { return m_sd; };
		bool ShouldDelete() { return m_delete_flag; };
		bool IsTimeout() { return m_timeout_flag; }
		INetDriver *GetDriver() { return mp_driver; };

		virtual OS::MetricInstance GetMetrics() = 0;

		OS::Address getAddress() { return m_sd->address; };
	protected:
		INetIOSocket *m_sd;
		INetDriver *mp_driver;
		struct timeval m_last_recv, m_last_ping;

		bool m_delete_flag;
		bool m_timeout_flag;

		OS::CMutex *mp_mutex;
	private:

	};
#endif