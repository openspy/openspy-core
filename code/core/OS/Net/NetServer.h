#ifndef _INETSERVER_H
#define _INETSERVER_H
#include <vector>
#include <OS/OpenSpy.h>
#include "NetDriver.h"
class INetServer {
public:
	INetServer();
	virtual ~INetServer();
	virtual void tick();
	/*
		Currently the driver is aware of what type of NetServer its connected to.
	*/
	void addNetworkDriver(INetDriver *driver);
protected:
	void NetworkTick(); //fires the INetEventMgr
//private:
	std::vector<INetDriver *> m_net_drivers;
};
#endif //_IGAMESERVER_H