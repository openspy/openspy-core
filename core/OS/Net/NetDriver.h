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
	virtual void tick(fd_set *fdset) = 0;
	virtual void think(fd_set *fdset) = 0;
	virtual int getListenerSocket() = 0;
	INetServer *getServer() { return m_server; }

	
	////////////////////////
	// required by SelectNetEventManager
	//  returns highest socket 
	virtual int setup_fdset(fd_set *fdset) = 0;
protected:
	INetServer *m_server;
};
#endif //_NETDRIVER_H