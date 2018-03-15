#ifndef _INETSERVER_H
#define _INETSERVER_H
#include <vector>
#include <OS/OpenSpy.h>
#include <OS/Analytics/Metric.h>
#include "NetDriver.h"
#include "NetEventManager.h"
#include "NetIOInterface.h"
class INetServer {
public:
	INetServer();
	virtual ~INetServer();
	virtual void init() = 0;
	virtual void tick();
	virtual void shutdown() = 0;
	/*
		Currently the driver is aware of what type of NetServer its connected to.
	*/
	void addNetworkDriver(INetDriver *driver);
	void flagExit();

	void RegisterSocket(INetPeer *peer);
	void UnregisterSocket(INetPeer *peer);

	virtual OS::MetricInstance GetMetrics() = 0;

	INetIOInterface *getNetIOInterface();
protected:
	void NetworkTick(); //fires the INetEventMgr
//private:
	INetEventManager *mp_net_event_mgr;
	INetIOInterface *mp_net_io_interface;
	std::vector<INetDriver *> m_net_drivers;
};
#endif //_IGAMESERVER_H