#ifndef _NETDRIVER_H
#define _NETDRIVER_H
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/LinkedList.h>
#include <uv.h>
#include <stack>
class INetServer;
class INetPeer;
class INetDriver {
public:
	INetDriver(INetServer *server);
	virtual ~INetDriver();
	/*
		Check for incoming data, etc
	*/
	virtual void think() = 0;
	INetServer *getServer() { return m_server; }
	struct sockaddr_in GetAddress() { return m_recv_addr; };
protected:
	INetServer *m_server;
	struct sockaddr_in m_recv_addr;
};
#endif //_NETDRIVER_H