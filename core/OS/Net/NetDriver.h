#ifndef _NETDRIVER_H
#define _NETDRIVER_H
#include <OS/OpenSpy.h>
class INetServer;
class INetDriver {
public:
	INetDriver(INetServer *server);
	virtual ~INetDriver();
	/*
		Check for incoming data, etc
	*/
	virtual void think(bool listen_waiting) = 0;
	virtual int getListenerSocket() = 0;
	virtual const std::vector<int> getSockets() = 0;
	INetServer *getServer() { return m_server; }
protected:
	INetServer *m_server;
};
#endif //_NETDRIVER_H